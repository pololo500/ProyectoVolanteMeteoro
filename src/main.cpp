#include <Arduino.h>
#include <WiFi.h>
#include "PubSubClient.h"
#include <ArduinoJson.h>

// ========================== MACROS ==========================
// Habilitacion de debug para impresion por puerto serial
#define SERIAL_DEBUG_ENABLED 1

#if SERIAL_DEBUG_ENABLED
#define DebugPrint(str)      \
	{                        \
		Serial.println(str); \
	}
#endif

#define DebugPrintEstado(estado, evento)                                     \
	{                                                                        \
		DebugPrint("-----------------------------------------------------"); \
		Serial.print("EST-> [");                                             \
		Serial.print(estado);                                                \
		Serial.print("]: EVT-> [");                                          \
		Serial.print(evento);                                                \
		Serial.println("].");                                                \
		DebugPrint("-----------------------------------------------------"); \
	}

#define MAX_STATES 5
#define MAX_EVENTS 8

// ========================== CONSTANTES ==========================
// CAMBIAR ESTA CONSTANTE SI SE USA WOKWI O NO
const bool WOKWI = true;
// WOKWI = FALSE 	-> VSCODE
// WOKWI = TRUE 	-> WOKWI

// ADEMÁS, modificar según se necesite la
// configuración PWM buzzer en setup()

// Pines (ESP32 DevKit V1)
const int PIN_BUZZER = 21;
const int PIN_MOTOR_VIBRADOR = 32;
const int PIN_FSR_IZQ = 33;
const int PIN_FSR_DER = 34;
const int PIN_VOLANTE = 35;

// Buzzer
const int BUZZER_OFF = 0;
const int BUZZER_INTERMITENTE = 1;
const int BUZZER_CONTINUO = 2;

// PWM
const int FREQ_ALERTA = 1000;
const int FREQ_EMERGENCIA = 2000;
const int BUZZER_RESOLUTION = 8;
// Esta constante es para VS
const int BUZZER_CHANNEL = 0;

// FreeRTOS
const int STACK_SIZE_2048 = 2048;
const int STACK_SIZE_4096 = 4096;
const int SENSOR_TASK_PRIORITY = 3;
const int BUZZER_TASK_PRIORITY = 1;
const int WIFI_TASK_PRIORITY = 2;
const int BUZZER_QUEUE_SIZE = 1;

// Tiempos
const int UMBRAL_DIFERENCIA_TIMEOUT = 120;
const int UMBRAL_TIMEOUT_ALERTA = 2500;
const int UMBRAL_TIMEOUT_ERROR = 5000;
const int UMBRAL_TIEMPO_UNA_MANO = 2000;

const int SENSOR_TASK_DELAY = 20;
const int BUZZER_DELAY_OFF = 100;
const int BUZZER_DELAY_ON = 300;
const int BUZZER_DELAY_CONTINUO = 100;
const int WIFI_TASK_DELAY = 100;
const int RECONNECT_DELAY = 5000;
const int SENSOR_PUBLISH_INTERVAL = 1000;

// Varibles globales
int umbralMano = 1366;
int umbralMovimientoLeve = 80;
int umbralMovimientoBrusco = 260;
volatile bool gAlarmaSolicitada = false;

// Wifi y MQTT
const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* MQTT_SERVER = "broker.emqx.io";
const int MQTT_PORT = 1883;

WiFiClient espClient;
PubSubClient client(espClient);
// Topicos
const char* TOPIC_ESTADO   = "volante/estado";
const char* TOPIC_SENSOR   = "volante/sensores";
const char* TOPIC_COMANDOS = "volante/comandos"; //comandos desde el celular

// ========================== ENUMS ==========================
enum states
{
	ST_INIT,
	ST_DETECTANDO,
	ST_ALERTA_LEVE,
	ST_ALERTA_FUERTE,
	ST_ERROR
} current_state;

const char *states_s[] = {
	"ST_Init",
	"ST_Detectando",
	"ST_AlertaLeve",
	"ST_AlertaFuerte",
	"ST_ERROR"};

enum events
{
	EV_CONT,
	EV_DUMMY,
	EV_UNA_SOLA_MANO,
	EV_MANIOBRA_SINUOSA_LEVE,
	EV_MANIOBRA_SINUOSA_BRUSCA,
	EV_SIN_MANOS,
	EV_TIMEOUT,
	EV_UNKNOW
} new_event;

const char *events_s[] = {
	"EV_CONT",
	"EV_Dummy",
	"EV_Una_sola_mano",
	"EV_Maniobra_sinuosa_leve",
	"EV_Maniobra_sinuosa_brusca",
	"EV_Sin_manos",
	"EV_Timeout",
	"EV_UNKNOW"};

// ========================== ESTRUCTURAS ==========================
struct stLectura
{
	int fsrIzq;
	int fsrDer;
	int volante;
	int difVolante;
	bool manoIzq;
	bool manoDer;
	bool unaMano;
	bool dosManos;
	bool sinManos;
	bool maniobraLeve;
	bool maniobraBrusca;
	bool volanteEstabilizado;
};

// ========================== DEF. FUNCIONES ==========================
void none();
void irInit();
void irDetectando();
void irAlertaLeve();
void irAlertaFuerte();
void irError();
String generarJsonSensores();

typedef void (*transition)();

// ========================== VARIABLES GLOBALES ==========================
// FreeRTOS
QueueHandle_t buzzerQueue;
volatile int buzzer_mode = BUZZER_OFF;

// Lógica
stLectura gLectura;
int gValorVolanteAnterior = 0;
unsigned long gLastControlTick = 0;
unsigned long gStateEntryTick = 0;
unsigned long gUnaManoDesde = 0;

// Matriz de transición de estados
transition state_table[MAX_STATES][MAX_EVENTS] =
	{
		/*Estado*/																								   /*EV_CONT,  EV_Dummy,		EV_Una_sola_mano,	EV_Maniobra_sinuosa_leve,	EV_Maniobra_sinuosa_brusca,	EV_Sin_manos,	EV_Timeout,		EV_UNKNOW */
		/*ST_INIT*/ {none, irDetectando, none, none, none, none, none, irError},								   // ST_INIT
		/*ST_DETECTANDO*/ {none, none, irAlertaLeve, irAlertaLeve, irAlertaFuerte, irAlertaFuerte, none, irError}, // ST_DETECTANDO
		/*ST_ALERTA_LEVE*/ {none, none, none, none, irAlertaFuerte, irAlertaFuerte, irDetectando, irError},		   // ST_ALERTA_LEVE
		/*ST_ALERTA_FUERTE*/ {none, none, none, none, none, none, irDetectando, irError},						   // ST_ALERTA_FUERTE
		/*ST_ERROR*/ {none, none, none, none, none, none, irInit, irError}										   // ST_ERROR
};

// ========================== FUNCIONES ==========================
// De manejo de componentes
void setearLed(bool encendido)
{
	digitalWrite(PIN_MOTOR_VIBRADOR, encendido ? HIGH : LOW);
}

void enviarBuzzer(int modo)
{
	xQueueOverwrite(buzzerQueue, &modo);
}

void apagarBuzzer()
{
	enviarBuzzer(BUZZER_OFF);
}

void emitirNotaLeve()
{
	enviarBuzzer(BUZZER_INTERMITENTE);
}

void emitirCancionFuerte()
{
	enviarBuzzer(BUZZER_CONTINUO);
}

// De eventos de la Máquina de Estados
void none()
{
}

void irInit()
{
	apagarBuzzer();
	setearLed(false);
	current_state = ST_INIT;
	gStateEntryTick = millis();
	if(client.connected())
		client.publish(TOPIC_ESTADO,"INIT");
}

void irDetectando()
{
	apagarBuzzer();
	setearLed(false);
	current_state = ST_DETECTANDO;
	gStateEntryTick = millis();
	if(client.connected())
		client.publish(TOPIC_ESTADO,"DETECTANDO");
}

void irAlertaLeve()
{
	setearLed(false);
	emitirNotaLeve();
	current_state = ST_ALERTA_LEVE;
	gStateEntryTick = millis();
	if(client.connected())
		client.publish(TOPIC_ESTADO,"ALERTA_LEVE");
}

void irAlertaFuerte()
{
	setearLed(true);
	emitirCancionFuerte();
	current_state = ST_ALERTA_FUERTE;
	gStateEntryTick = millis();
	if(client.connected())
		client.publish(TOPIC_ESTADO,"ALERTA_FUERTE");
}

void irError()
{
	setearLed(true);
	emitirCancionFuerte();
	current_state = ST_ERROR;
	gStateEntryTick = millis();
	if(client.connected())
		client.publish(TOPIC_ESTADO,"ERROR");
}

void actualizarLecturas()
{
	// Lectura de sensores
	gLectura.fsrIzq = analogRead(PIN_FSR_IZQ);
	gLectura.fsrDer = analogRead(PIN_FSR_DER);
	gLectura.volante = analogRead(PIN_VOLANTE);

	// Calculo de diferencia de volante respecto a lectura anterior
	gLectura.difVolante = abs(gLectura.volante - gValorVolanteAnterior);
	gValorVolanteAnterior = gLectura.volante;

	// Clasificacion de manos en true o false segun umbral predefinido
	gLectura.manoIzq = (gLectura.fsrIzq >= umbralMano);
	gLectura.manoDer = (gLectura.fsrDer >= umbralMano);
	gLectura.unaMano = (gLectura.manoIzq ^ gLectura.manoDer);
	gLectura.dosManos = (gLectura.manoIzq && gLectura.manoDer);
	gLectura.sinManos = (!gLectura.manoIzq && !gLectura.manoDer);

	// Clasificacion de maniobras en true o false segun umbrales predefinidos
	gLectura.maniobraLeve = (gLectura.difVolante >= umbralMovimientoLeve && gLectura.difVolante < umbralMovimientoBrusco);
	gLectura.maniobraBrusca = (gLectura.difVolante >= umbralMovimientoBrusco);
	gLectura.volanteEstabilizado = (gLectura.difVolante < umbralMovimientoLeve);
}

// De la Máquina de Estados
events get_new_event()
{
	unsigned long ct = millis();
	unsigned long diferencia = ct - gLastControlTick;

	// Timeout de control para evitar procesar eventos muy seguidos
	if (diferencia < UMBRAL_DIFERENCIA_TIMEOUT)
		return EV_CONT;

	if (gAlarmaSolicitada) // si llega comando del celular para activar alarma
  {
    gAlarmaSolicitada = false;
    return EV_SIN_MANOS;
  }

	gLastControlTick = ct;
	actualizarLecturas();

	if (gLectura.sinManos)
		return EV_SIN_MANOS;

	if (gLectura.maniobraBrusca)
		return EV_MANIOBRA_SINUOSA_BRUSCA;

	if (gLectura.unaMano)
	{
		if(gUnaManoDesde == 0)
			gUnaManoDesde = millis();

		if((millis() - gUnaManoDesde) >= UMBRAL_TIEMPO_UNA_MANO)
			return EV_UNA_SOLA_MANO;
	}
	else
		gUnaManoDesde = 0;

	if (gLectura.maniobraLeve)
		return EV_MANIOBRA_SINUOSA_LEVE;

	return EV_CONT;
}

events get_fsm_event()
{
	unsigned long ct = millis();

	switch (current_state)
	{
	case ST_INIT:
		return EV_DUMMY;

	case ST_ALERTA_LEVE:
	case ST_ALERTA_FUERTE:
		if ((ct - gStateEntryTick) >= UMBRAL_TIMEOUT_ALERTA)
			return EV_TIMEOUT;
		break;

	case ST_ERROR:
		if ((ct - gStateEntryTick) >= UMBRAL_TIMEOUT_ERROR)
			return EV_TIMEOUT;
		break;

	default:
		break;
	}

	return get_new_event();
}

// Principal
// Maquina de estados para deteccion de manos en volante con eventos segun lecturas de sensores y estado actual
void maquina_estados_deteccion_manos()
{
	new_event = get_fsm_event();

	if ((new_event >= 0) && (new_event < MAX_EVENTS) && (current_state >= 0) && (current_state < MAX_STATES))
	{
		if (new_event != EV_CONT)
			DebugPrintEstado(states_s[current_state], events_s[new_event]);

		state_table[current_state][new_event]();
	}
	else
	{
		DebugPrintEstado(states_s[ST_ERROR], events_s[EV_UNKNOW]);
		irError();
	}

	new_event = EV_CONT;
}

// De Tareas FreeRTOS
void sensor_task(void *pv)
{
	unsigned long lastPublish = 0;
	while (true)
	{
		maquina_estados_deteccion_manos();
		String json = generarJsonSensores();
		if (millis() - lastPublish >= SENSOR_PUBLISH_INTERVAL){
			if(client.connected())
				client.publish(TOPIC_SENSOR,json.c_str());
			lastPublish = millis();
		}
		vTaskDelay(pdMS_TO_TICKS(SENSOR_TASK_DELAY));
	}
}

void buzzer_task(void *pv)
{
	int modo;

	while (true)
	{
		// Leer desde la FSM
		if (xQueueReceive(buzzerQueue, &modo, 0) == pdPASS)
			buzzer_mode = modo;

		switch (buzzer_mode)
		{
		case BUZZER_OFF: // Apagado
			if (!WOKWI)
			{
				// Esta configuración es para VS Code:
				ledcWrite(BUZZER_CHANNEL, 0);
				vTaskDelay(pdMS_TO_TICKS(BUZZER_DELAY_OFF));
			}
			else
			{

				// Esta configuración es para Wokwi
				ledcWrite(PIN_BUZZER, 0);
				vTaskDelay(pdMS_TO_TICKS(BUZZER_DELAY_OFF));
			}
			break;

		case BUZZER_INTERMITENTE: // Intermitente (ST_ALERTA_LEVE)
			if (!WOKWI)
			{
				// Esta configuración es para VS Code:
				ledcWriteTone(BUZZER_CHANNEL, FREQ_ALERTA);
				vTaskDelay(pdMS_TO_TICKS(BUZZER_DELAY_ON)); // 300ms ON
				ledcWrite(BUZZER_CHANNEL, 0);
				vTaskDelay(pdMS_TO_TICKS(BUZZER_DELAY_ON)); // 300ms OFF
			}
			else
			{
				// Esta configuración es para Wokwi:
				ledcWriteTone(PIN_BUZZER, FREQ_ALERTA);
				vTaskDelay(pdMS_TO_TICKS(BUZZER_DELAY_ON)); // 300ms ON
				ledcWrite(PIN_BUZZER, 0);
				vTaskDelay(pdMS_TO_TICKS(BUZZER_DELAY_ON)); // 300ms OFF
			}
			break;

		case BUZZER_CONTINUO: // Continuo (ST_ALERTA_FUERTE)
			if(!WOKWI){
				// Esta configuración es para VS Code:
				ledcWriteTone(BUZZER_CHANNEL, FREQ_EMERGENCIA);
				vTaskDelay(pdMS_TO_TICKS(BUZZER_DELAY_CONTINUO)); // Simplemente mantenerlo encendido
			}else{
				// Esta configuración es para Wokwi:
				ledcWriteTone(PIN_BUZZER, FREQ_EMERGENCIA);
				vTaskDelay(pdMS_TO_TICKS(BUZZER_DELAY_CONTINUO)); // Simplemente mantenerlo encendido
			}
			break;
		}
	}
}

void wifi_task(void *pv){
	while(true){
		if (WiFi.status() != WL_CONNECTED)// Verifica WiFi 
			conectarWiFi(); 
		if (WiFi.status() == WL_CONNECTED)//verifica mqtt solo si hay wifi
		{
			if (!client.connected())
				conectarMQTT();
			else
				client.loop();//mantiene el mqtt funcionando
		}
		vTaskDelay(pdMS_TO_TICKS(WIFI_TASK_DELAY));
	} 
}

// WIFI y MQTT
void conectarWiFi()
{
  int intentos=0;
	DebugPrint("Conectando WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED && intentos<20)
  {
    vTaskDelay(pdMS_TO_TICKS(500));
		intentos++;
  }

  if (WiFi.status() == WL_CONNECTED){
    DebugPrint("WiFi conectado");
	}
	else{
    DebugPrint("Error WiFi");
	}
}

void conectarMQTT()
{
  static int intentos = 0;

  if (client.connect("ESP32_VOLANTE"))
  {
    DebugPrint("MQTT conectado");

    client.subscribe(TOPIC_COMANDOS);
    intentos = 0;
  }
  else
  {
    intentos++;
    DebugPrint("Error MQTT");

    // Si falla muchas veces, esperar más tiempo
    if (intentos >= 5)
    {
      intentos = 0;
      vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY));
    }
    else
      vTaskDelay(pdMS_TO_TICKS(2000));
  }
}


void callback(char* topic, byte* message, unsigned int length)
{
  String stMensaje;
  
  for (int i = 0; i < length; i++){
    stMensaje += (char)message[i];
  }

  DebugPrint(stMensaje);

	stMensaje.trim();//sacacar salto de linea que puede romper el mensaje
	if (strcmp(topic, TOPIC_COMANDOS) != 0)// verificar que sea el topico de comandos
  	return;
  
  if (stMensaje == "ALARMA")
    gAlarmaSolicitada = true;
	else if (stMensaje.startsWith("UMBRAL_MANO:"))
  {
    String valor = stMensaje.substring(12);
		int nuevoValor = valor.toInt();
		if(nuevoValor >= 500 && nuevoValor <= 3000)
    	umbralMano = nuevoValor;

    DebugPrint("Nuevo umbral mano: ");
    DebugPrint(umbralMano);
  }
	else if (stMensaje.startsWith("UMBRAL_LEVE:"))
  {
    String valor = stMensaje.substring(12);
		int nuevoValor = valor.toInt();
		if(nuevoValor >= 20 && nuevoValor <= 300)
    	umbralMovimientoLeve = nuevoValor;

    DebugPrint("Nuevo umbral leve: ");
    DebugPrint(umbralMovimientoLeve);
  }
	else if (stMensaje.startsWith("UMBRAL_BRUSCO:"))
  {
    String valor = stMensaje.substring(14);
		int nuevoValor = valor.toInt();
		if(nuevoValor >= 100 && nuevoValor <= 1000 && nuevoValor > umbralMovimientoLeve)
    	umbralMovimientoBrusco = nuevoValor;

    DebugPrint("Nuevo umbral brusco: ");
    DebugPrint(umbralMovimientoBrusco);
  }
  
}

String generarJsonSensores(){
	StaticJsonDocument<256> doc;

	doc["estado"] = states_s[current_state];
	doc["fsrIzq"] = gLectura.fsrIzq;
	doc["fsrDer"] = gLectura.fsrDer;
	doc["volante"] = gLectura.volante;
	doc["timestamp"] = millis();

	String json;
	serializeJson(doc,json);

	return json;
}

// Básicas
void setup()
{
	// Configuracion de puerto serial para debug (115200 baudios)
	Serial.begin(115200);

	// Configuracion de pines
	pinMode(PIN_FSR_IZQ, INPUT);
	pinMode(PIN_FSR_DER, INPUT);
	pinMode(PIN_VOLANTE, INPUT);
	pinMode(PIN_BUZZER, OUTPUT);
	pinMode(PIN_MOTOR_VIBRADOR, OUTPUT);

	// Configuracion PWM buzzer
	if(!WOKWI)
	{
		// Esta configuración es para VS Code:
		//ledcSetup(BUZZER_CHANNEL, FREQ_ALERTA, BUZZER_RESOLUTION);
		//ledcAttachPin(PIN_BUZZER, BUZZER_CHANNEL);
	}
	else
	{
		// Esta configuración es para Wokwi:
		ledcAttach(PIN_BUZZER, FREQ_ALERTA, BUZZER_RESOLUTION);
		ledcWrite(PIN_BUZZER, 0);
	}

	// FreeRTOS
	buzzerQueue = xQueueCreate(BUZZER_QUEUE_SIZE, sizeof(int));

	// Wifi y MQTT
	conectarWiFi();
	client.setServer(MQTT_SERVER,MQTT_PORT);
	client.setCallback(callback);
	conectarMQTT();

	// Actualizar lecturas y tick de control
	gValorVolanteAnterior = analogRead(PIN_VOLANTE);
	gLastControlTick = millis();
	gStateEntryTick = millis();

	current_state = ST_INIT;
	new_event = EV_CONT;
	irInit();

	xTaskCreate(sensor_task, "SensorTask", STACK_SIZE_4096, NULL, SENSOR_TASK_PRIORITY, NULL);
	xTaskCreate(buzzer_task, "BuzzerTask", STACK_SIZE_2048, NULL, BUZZER_TASK_PRIORITY, NULL);
	xTaskCreate(wifi_task,"WifiTask", STACK_SIZE_4096, NULL, WIFI_TASK_PRIORITY, NULL);
}

void loop()
{
}