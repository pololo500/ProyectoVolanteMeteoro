#include <Arduino.h>

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
// Pines (ESP32 DevKit V1)
const int PIN_BUZZER = 21;
const int PIN_MOTOR_VIBRADOR = 32;
const int PIN_FSR_IZQ = 33;
const int PIN_FSR_DER = 34;
const int PIN_VOLANTE = 35;

// Umbrales
const int UMBRAL_MANO = 1366;
const int UMBRAL_MOVIMIENTO_LEVE = 80;
const int UMBRAL_MOVIMIENTO_BRUSCO = 260;

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
const int BUZZER_STACK_SIZE = 2048;
const int SENSORES_STACK_SIZE = 4096;
const int SENSOR_TASK_PRIORITY = 2;
const int BUZZER_TASK_PRIORITY = 1;
const int BUZZER_QUEUE_SIZE = 1;

// Tiempos
const int UMBRAL_DIFERENCIA_TIMEOUT = 120;
const int UMBRAL_TIMEOUT_ALERTA = 2500;
const int UMBRAL_TIMEOUT_ERROR = 5000;

const int SENSOR_TASK_DELAY = 20;
const int BUZZER_DELAY_OFF = 100;
const int BUZZER_DELAY_ON = 300;
const int BUZZER_DELAY_CONTINUO = 100;

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

// Matriz de transición de estados
transition state_table[MAX_STATES][MAX_EVENTS] =
{
	/*Estado*/				/*EV_CONT,  EV_Dummy,		EV_Una_sola_mano,	EV_Maniobra_sinuosa_leve,	EV_Maniobra_sinuosa_brusca,	EV_Sin_manos,	EV_Timeout,		EV_UNKNOW */
	/*ST_INIT*/ 			{none, 		irDetectando, 	none, 				none, 						none, 						none, 			none, 			irError}, // ST_INIT
	/*ST_DETECTANDO*/ 		{none,		none, 			irAlertaLeve, 		irAlertaLeve, 				irAlertaFuerte, 			irAlertaFuerte, none, 			irError}, // ST_DETECTANDO
	/*ST_ALERTA_LEVE*/ 		{none, 		none, 			none, 				none, 						irAlertaFuerte, 			irAlertaFuerte, irDetectando, 	irError}, // ST_ALERTA_LEVE
	/*ST_ALERTA_FUERTE*/	{none, 		none, 			none, 				none, 						none, 						none, 			irDetectando, 	irError}, // ST_ALERTA_FUERTE
	/*ST_ERROR*/ 			{none, 		none, 			none, 				none, 						none, 						none, 			irInit, 		irError}  // ST_ERROR
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
}

void irDetectando()
{
	apagarBuzzer();
	setearLed(false);
	current_state = ST_DETECTANDO;
	gStateEntryTick = millis();
}

void irAlertaLeve()
{
	setearLed(false);
	emitirNotaLeve();
	current_state = ST_ALERTA_LEVE;
	gStateEntryTick = millis();
}

void irAlertaFuerte()
{
	setearLed(true);
	emitirCancionFuerte();
	current_state = ST_ALERTA_FUERTE;
	gStateEntryTick = millis();
}

void irError()
{
	setearLed(true);
	emitirCancionFuerte();
	current_state = ST_ERROR;
	gStateEntryTick = millis();
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
	gLectura.manoIzq = (gLectura.fsrIzq >= UMBRAL_MANO);
	gLectura.manoDer = (gLectura.fsrDer >= UMBRAL_MANO);
	gLectura.unaMano = (gLectura.manoIzq ^ gLectura.manoDer);
	gLectura.dosManos = (gLectura.manoIzq && gLectura.manoDer);
	gLectura.sinManos = (!gLectura.manoIzq && !gLectura.manoDer);

	// Clasificacion de maniobras en true o false segun umbrales predefinidos
	gLectura.maniobraLeve = (gLectura.difVolante >= UMBRAL_MOVIMIENTO_LEVE && gLectura.difVolante < UMBRAL_MOVIMIENTO_BRUSCO);
	gLectura.maniobraBrusca = (gLectura.difVolante >= UMBRAL_MOVIMIENTO_BRUSCO);
	gLectura.volanteEstabilizado = (gLectura.difVolante < UMBRAL_MOVIMIENTO_LEVE);
}

// De la Máquina de Estados
events detectarEventoDetectando()
{
	if (gLectura.sinManos)
		return EV_SIN_MANOS;
	if (gLectura.maniobraBrusca)
		return EV_MANIOBRA_SINUOSA_BRUSCA;
	if (gLectura.unaMano)
		return EV_UNA_SOLA_MANO;
	if (gLectura.maniobraLeve)
		return EV_MANIOBRA_SINUOSA_LEVE;

	return EV_CONT;
}

events detectarEventoAlertaLeve(unsigned long ct)
{
	if (gLectura.sinManos)
		return EV_SIN_MANOS;
	if (gLectura.maniobraBrusca)
		return EV_MANIOBRA_SINUOSA_BRUSCA;

	if ((ct - gStateEntryTick) >= UMBRAL_TIMEOUT_ALERTA)
		return EV_TIMEOUT;

	return EV_CONT;
}

events detectarEventoAlertaFuerte(unsigned long ct)
{
	if ((ct - gStateEntryTick) >= UMBRAL_TIMEOUT_ALERTA)
		return EV_TIMEOUT;

	return EV_CONT;
}

events detectarEventoError(unsigned long ct)
{
	if ((ct - gStateEntryTick) >= UMBRAL_TIMEOUT_ERROR)
		return EV_TIMEOUT;

	return EV_CONT;
}

void get_new_event()
{
	if (current_state == ST_INIT)
	{
		new_event = EV_DUMMY;
		return;
	}

	// Timeout de control para evitar procesar eventos muy seguidos
	unsigned long ct = millis();
	unsigned long diferencia = ct - gLastControlTick;

	// Evitar overflow de millis() y procesar eventos muy seguidos
	if (diferencia < UMBRAL_DIFERENCIA_TIMEOUT)
	{
		new_event = EV_CONT;
		return;
	}

	// Actualizar lecturas y tick de control
	gLastControlTick = ct;
	actualizarLecturas();

	// Determinar nuevo evento segun estado actual y lecturas
	switch (current_state)
	{
	case ST_DETECTANDO:
		new_event = detectarEventoDetectando();
		break;

	case ST_ALERTA_LEVE:
		new_event = detectarEventoAlertaLeve(ct);
		break;

	case ST_ALERTA_FUERTE:
		new_event = detectarEventoAlertaFuerte(ct);
		break;

	case ST_ERROR:
		new_event = detectarEventoError(ct);
		break;

	default:
		new_event = EV_UNKNOW;
		break;
	}
}

// Principal
// Maquina de estados para deteccion de manos en volante con eventos segun lecturas de sensores y estado actual
void maquina_estados_deteccion_manos()
{
	get_new_event();

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
	while (true)
	{
		maquina_estados_deteccion_manos();
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
			// Esta configuración es para VS Code:
			// ledcWrite(BUZZER_CHANNEL, 0);
			// vTaskDelay(pdMS_TO_TICKS(BUZZER_DELAY_OFF));

			// Esta configuración es para Wokwi
			ledcWrite(PIN_BUZZER, 0);
			vTaskDelay(pdMS_TO_TICKS(BUZZER_DELAY_OFF));
			break;

		case BUZZER_INTERMITENTE: // Intermitente (ST_ALERTA_LEVE)
			// Esta configuración es para VS Code:
			ledcWriteTone(BUZZER_CHANNEL, FREQ_ALERTA);
			vTaskDelay(pdMS_TO_TICKS(BUZZER_DELAY_ON)); // 300ms ON
			ledcWrite(BUZZER_CHANNEL, 0);
			vTaskDelay(pdMS_TO_TICKS(BUZZER_DELAY_ON)); // 300ms OFF

			// Esta configuración es para Wokwi:
			// ledcWriteTone(PIN_BUZZER, FREQ_ALERTA);
			// vTaskDelay(pdMS_TO_TICKS(BUZZER_DELAY_ON)); // 300ms ON
			// ledcWrite(PIN_BUZZER, 0);
			// vTaskDelay(pdMS_TO_TICKS(BUZZER_DELAY_ON)); // 300ms OFF
			break;

		case BUZZER_CONTINUO: // Continuo (ST_ALERTA_FUERTE)
			// Esta configuración es para VS Code:
			ledcWriteTone(BUZZER_CHANNEL, FREQ_EMERGENCIA);
			vTaskDelay(pdMS_TO_TICKS(BUZZER_DELAY_CONTINUO)); // Simplemente mantenerlo encendido

			// Esta configuración es para Wokwi:
			// ledcWriteTone(PIN_BUZZER, FREQ_EMERGENCIA);
			// vTaskDelay(pdMS_TO_TICKS(BUZZER_DELAY_CONTINUO)); // Simplemente mantenerlo encendido
			break;
		}
	}
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
	// Esta configuración es para VS Code:
	ledcSetup(BUZZER_CHANNEL, FREQ_ALERTA, BUZZER_RESOLUTION);
	ledcAttachPin(PIN_BUZZER, BUZZER_CHANNEL);

	// Esta configuración es para Wokwi:
	// ledcAttach(PIN_BUZZER, FREQ_ALERTA, BUZZER_RESOLUTION);
	// ledcWrite(PIN_BUZZER, 0);

	// FreeRTOS
	buzzerQueue = xQueueCreate(BUZZER_QUEUE_SIZE, sizeof(int));

	// Actualizar lecturas y tick de control
	gValorVolanteAnterior = analogRead(PIN_VOLANTE);
	gLastControlTick = millis();
	gStateEntryTick = millis();

	current_state = ST_INIT;
	new_event = EV_CONT;
	irInit();

	xTaskCreate(sensor_task, "SensorTask", SENSORES_STACK_SIZE, NULL, SENSOR_TASK_PRIORITY, NULL);
	xTaskCreate(buzzer_task, "BuzzerTask", BUZZER_STACK_SIZE, NULL, BUZZER_TASK_PRIORITY, NULL);
}

void loop()
{
}