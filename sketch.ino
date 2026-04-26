#include <Arduino.h>

// Habilitacion de debug para impresion por puerto serial
#define SERIAL_DEBUG_ENABLED 1

#if SERIAL_DEBUG_ENABLED
  #define DebugPrint(str) \
    { \
      Serial.println(str); \
    }
#else
  #define DebugPrint(str)
#endif

#define DebugPrintEstado(estado, evento) \
  { \
    String est = estado; \
    String evt = evento; \
    String str; \
    str = "-----------------------------------------------------"; \
    DebugPrint(str); \
    str = "EST-> [" + est + "]: EVT-> [" + evt + "]."; \
    DebugPrint(str); \
    str = "-----------------------------------------------------"; \
    DebugPrint(str); \
  }

// Pines (ESP32 DevKit V1)
const int PIN_FSR_IZQ = 34;
const int PIN_FSR_DER = 35;
const int PIN_VOLANTE = 32;
const int PIN_BUZZER = 33;
const int PIN_MOTOR_VIBRADOR = 27;

// Umbrales
const int UMBRAL_MANO = 1366;
const int UMBRAL_MOVIMIENTO_LEVE = 80;
const int UMBRAL_MOVIMIENTO_BRUSCO = 260;

// Notas buzzer
const int NOTA_DO = 262;
const int NOTA_MI = 330;
const int NOTA_SOL = 392;
const int NOTA_DO_ALTO = 523;

// Tiempos
const int UMBRAL_DIFERENCIA_TIMEOUT = 120;
const int UMBRAL_TIMEOUT_ALERTA = 2500;

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

stLectura gLectura;
int gValorVolanteAnterior = 0;
long gLastControlTick = 0;
long gStateEntryTick = 0;

void none();
void irInit();
void irDetectando();
void irAlertaLeve();
void irAlertaFuerte();
void irError();

enum states
{
  ST_INIT,
  ST_DETECTANDO,
  ST_ALERTA_LEVE,
  ST_ALERTA_FUERTE,
  ST_ERROR
} current_state;

String states_s[] = {
  "ST_Init",
  "ST_Detectando",
  "ST_AlertaLeve",
  "ST_AlertaFuerte",
  "ST_ERROR"
};

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

String events_s[] = {
  "EV_CONT",
  "EV_Dummy",
  "EV_Una_sola_mano",
  "EV_Maniobra_sinuosa_leve",
  "EV_Maniobra_sinuosa_brusca",
  "EV_Sin_manos",
  "EV_Timeout",
  "EV_UNKNOW"
};

#define MAX_STATES 5
#define MAX_EVENTS 8

typedef void (*transition)();

transition state_table[MAX_STATES][MAX_EVENTS] =
{
  /*Estado*/           /*EV_CONT,  EV_Dummy,     EV_Una_sola_mano, EV_Maniobra_sinuosa_leve, EV_Maniobra_sinuosa_brusca, EV_Sin_manos,   EV_Timeout,   EV_UNKNOW */
  /*ST_INIT*/          { none,     irDetectando, none,             none,                     none,                       none,           none,         irError },// ST_INIT
  /*ST_DETECTANDO*/    { none,     none,         irAlertaLeve,     irAlertaLeve,             irAlertaFuerte,             irAlertaFuerte, none,         irError },// ST_DETECTANDO
  /*ST_ALERTA_LEVE*/   { none,     none,         none,             none,                     irAlertaFuerte,             irAlertaFuerte, irDetectando, irError },// ST_ALERTA_LEVE
  /* ST_ALERTA_FUERTE*/{ none,     none,         none,             none,                     none,                       none,           irDetectando, irError },// ST_ALERTA_FUERTE
  /*ST_ERROR*/         { irError,  irError,      irError,          irError,                  irError,                    irError,        irError,      irError }// ST_ERROR
};

void setearMotorVibrador(bool encendido)
{
  digitalWrite(PIN_MOTOR_VIBRADOR, encendido ? HIGH : LOW);
}

void apagarBuzzer()
{
  noTone(PIN_BUZZER);
}

void emitirNotaLeve()
{
  tone(PIN_BUZZER, NOTA_MI, 140);
}

void emitirCancionFuerte()
{
  tone(PIN_BUZZER, NOTA_DO, 100);
  delay(110);
  tone(PIN_BUZZER, NOTA_SOL, 110);
  delay(120);
  tone(PIN_BUZZER, NOTA_DO_ALTO, 140);
}

void actualizarLecturas()
{
  // Lectura de sensores
  gLectura.fsrIzq   =   analogRead(PIN_FSR_IZQ);
  gLectura.fsrDer   =   analogRead(PIN_FSR_DER);
  gLectura.volante  =  analogRead(PIN_VOLANTE);

  // Calculo de diferencia de volante respecto a lectura anterior
  gLectura.difVolante   = abs(gLectura.volante - gValorVolanteAnterior);
  gValorVolanteAnterior = gLectura.volante;

  // Clasificacion de manos en true o false segun umbral predefinido
  gLectura.manoIzq  = (gLectura.fsrIzq >= UMBRAL_MANO);
  gLectura.manoDer  = (gLectura.fsrDer >= UMBRAL_MANO);
  gLectura.unaMano  = (gLectura.manoIzq ^ gLectura.manoDer);
  gLectura.dosManos = (gLectura.manoIzq && gLectura.manoDer);
  gLectura.sinManos = (!gLectura.manoIzq && !gLectura.manoDer);

  // Clasificacion de maniobras en true o false segun umbrales predefinidos
  gLectura.maniobraLeve        = (gLectura.difVolante >= UMBRAL_MOVIMIENTO_LEVE && gLectura.difVolante < UMBRAL_MOVIMIENTO_BRUSCO);
  gLectura.maniobraBrusca      = (gLectura.difVolante >= UMBRAL_MOVIMIENTO_BRUSCO);
  gLectura.volanteEstabilizado = (gLectura.difVolante < UMBRAL_MOVIMIENTO_LEVE);
}

void get_new_event()
{
  if (current_state == ST_INIT)
  {
    new_event = EV_DUMMY;
    return;
  }

  // Timeout de control para evitar procesar eventos muy seguidos
  long ct = millis();
  int diferencia = (ct - gLastControlTick);

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
      if (gLectura.sinManos)
      {
        new_event = EV_SIN_MANOS;
      }
      else if (gLectura.maniobraBrusca)
      {
        new_event = EV_MANIOBRA_SINUOSA_BRUSCA;
      }
      else if (gLectura.unaMano)
      {
        new_event = EV_UNA_SOLA_MANO;
      }
      else if (gLectura.maniobraLeve)
      {
        new_event = EV_MANIOBRA_SINUOSA_LEVE;
      }
      else
      {
        new_event = EV_CONT;
      }
      break;

    case ST_ALERTA_LEVE:
      if (gLectura.sinManos)
      {
        new_event = EV_SIN_MANOS;
      }
      else if (gLectura.maniobraBrusca)
      {
        new_event = EV_MANIOBRA_SINUOSA_BRUSCA;
      }
      else if ((ct - gStateEntryTick) >= UMBRAL_TIMEOUT_ALERTA)
      {
        new_event = EV_TIMEOUT;
      }
      else
      {
        new_event = EV_CONT;
      }
      break;

    case ST_ALERTA_FUERTE:
      if ((ct - gStateEntryTick) >= UMBRAL_TIMEOUT_ALERTA)
      {
        new_event = EV_TIMEOUT;
      }
      else
      {
        new_event = EV_CONT;
      }
      break;

    default:
      new_event = EV_UNKNOW;
      break;
  }
}

void none()
{
}

void irInit()
{
  apagarBuzzer();
  setearMotorVibrador(false);
  current_state = ST_INIT;
  gStateEntryTick = millis();
}

void irDetectando()
{
  apagarBuzzer();
  setearMotorVibrador(false);
  current_state = ST_DETECTANDO;
  gStateEntryTick = millis();
}

void irAlertaLeve()
{
  setearMotorVibrador(false);
  emitirNotaLeve();
  current_state = ST_ALERTA_LEVE;
  gStateEntryTick = millis();
}

void irAlertaFuerte()
{
  setearMotorVibrador(true);
  emitirCancionFuerte();
  current_state = ST_ALERTA_FUERTE;
  gStateEntryTick = millis();
}

void irError()
{
  setearMotorVibrador(true);
  tone(PIN_BUZZER, NOTA_DO_ALTO, 250);
  current_state = ST_ERROR;
  gStateEntryTick = millis();
}

// Ejecucion en loop, funcion principal
// Maquina de estados para deteccion de manos en volante con eventos segun lecturas de sensores y estado actual
void maquina_estados_deteccion_manos()
{
  get_new_event();

  if ((new_event >= 0) && (new_event < MAX_EVENTS) && (current_state >= 0) && (current_state < MAX_STATES))
  {
    if (new_event != EV_CONT)
    {
      DebugPrintEstado(states_s[current_state], events_s[new_event]);
    }

    state_table[current_state][new_event]();
  }
  else
  {
    DebugPrintEstado(states_s[ST_ERROR], events_s[EV_UNKNOW]);
    irError();
  }

  new_event = EV_CONT;
}

void setup()
{
  Serial.begin(115200);

  pinMode(PIN_FSR_IZQ, INPUT);
  pinMode(PIN_FSR_DER, INPUT);
  pinMode(PIN_VOLANTE, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_MOTOR_VIBRADOR, OUTPUT);

  gValorVolanteAnterior = analogRead(PIN_VOLANTE);
  gLastControlTick = millis();
  gStateEntryTick = millis();

  current_state = ST_INIT;
  new_event = EV_CONT;
  irInit();
}

void loop()
{
  maquina_estados_deteccion_manos();
}