# Documentacion del Sistema de Alerta de Atencion (ESP32)

## 1. Objetivo
Este proyecto implementa una maquina de estados finitos (FSM) para detectar perdida de atencion al volante usando:

- 2 sensores de fuerza (FSR) simulados con potenciometros
- 1 sensor de movimiento de volante (potenciometro)
- 1 buzzer para alerta sonora
- 1 salida digital (LED en Wokwi) que representa un motor vibrador real

Importante: el motor vibrador se controla en modo digital (ON/OFF), no por PWM.

---

## 2. Arquitectura general
El codigo esta organizado con arquitectura tipo matriz de transiciones:

- Enumeracion de estados
- Enumeracion de eventos
- Matriz state_table[estado][evento] de funciones de transicion
- Funcion central de ciclo: maquina_estados_deteccion_manos()
- Generador de eventos: get_new_event()

Flujo de ejecucion:

1. loop() llama a maquina_estados_deteccion_manos()
2. maquina_estados_deteccion_manos() llama a get_new_event()
3. Se selecciona la transicion desde state_table[current_state][new_event]
4. Se ejecuta accion de salida y cambio de estado
5. Se consume el evento y vuelve a empezar

---

## 3. Hardware y pines

- PIN_FSR_IZQ = 34
- PIN_FSR_DER = 35
- PIN_VOLANTE = 32
- PIN_BUZZER = 33
- PIN_MOTOR_VIBRADOR = 27

En Wokwi, el LED conectado a PIN 27 simula el motor vibrador.

---

## 4. Umbrales y criterios de deteccion

### 4.1 Deteccion de manos
- UMBRAL_MANO = 1366
- Si FSR >= UMBRAL_MANO, se considera que hay mano en ese lado.

Se derivan 3 condiciones:
- sinManos: ninguna mano detectada
- unaMano: exactamente una mano detectada
- dosManos: ambas manos detectadas

### 4.2 Deteccion de maniobra
Se usa la diferencia absoluta del valor del volante entre muestras consecutivas:

- difVolante = abs(volanteActual - volanteAnterior)

Umbrales:
- UMBRAL_MOVIMIENTO_LEVE = 80
- UMBRAL_MOVIMIENTO_BRUSCO = 260

Clasificacion:
- volanteEstabilizado: difVolante < 80
- maniobraLeve: 80 <= difVolante < 260
- maniobraBrusca: difVolante >= 260

### 4.3 Tiempos de control y timeout
- UMBRAL_DIFERENCIA_TIMEOUT = 120 ms (periodo minimo entre evaluaciones)
- UMBRAL_TIMEOUT_ALERTA = 2500 ms (genera EV_Timeout en estados de alerta)

---

## 5. Estados de la FSM (version actual)

1. ST_Init
2. ST_Detectando
3. ST_AlertaLeve
4. ST_AlertaFuerte
5. ST_ERROR

Descripcion funcional:

- ST_Init:
  - Estado de arranque
  - Buzzer apagado
  - Motor vibrador apagado
  - Sale por evento EV_Dummy hacia ST_Detectando

- ST_Detectando:
  - Buzzer apagado
  - Motor vibrador apagado
  - Estado base de monitoreo

- ST_AlertaLeve:
  - Buzzer emite 1 nota leve
  - Motor vibrador apagado

- ST_AlertaFuerte:
  - Buzzer emite secuencia ("cancion")
  - Motor vibrador encendido (salida digital HIGH)

- ST_ERROR:
  - Estado de falla
  - Buzzer y motor en patron de error

---

## 6. Eventos de la FSM (version actual)

1. EV_CONT
2. EV_Dummy
3. EV_Una_sola_mano
4. EV_Maniobra_sinuosa_leve
5. EV_Maniobra_sinuosa_brusca
6. EV_Sin_manos
7. EV_Timeout
8. EV_UNKNOW

Reglas:
- EV_CONT se usa cuando no hay cambio relevante.
- EV_Dummy se usa solo para la transicion inicial ST_Init -> ST_Detectando.
- EV_Timeout se genera por tiempo permanecido en estados de alerta.

---

## 7. Matriz de transiciones (resumen)

### 7.1 Desde ST_Init
- EV_Dummy -> ST_Detectando

### 7.2 Desde ST_Detectando
- EV_Una_sola_mano -> ST_AlertaLeve
- EV_Maniobra_sinuosa_leve -> ST_AlertaLeve
- EV_Maniobra_sinuosa_brusca -> ST_AlertaFuerte
- EV_Sin_manos -> ST_AlertaFuerte
- EV_CONT -> permanece

### 7.3 Desde ST_AlertaLeve
- EV_Maniobra_sinuosa_brusca -> ST_AlertaFuerte
- EV_Sin_manos -> ST_AlertaFuerte
- EV_Timeout -> ST_Detectando
- EV_CONT -> permanece

### 7.4 Desde ST_AlertaFuerte
- EV_Timeout -> ST_Detectando
- EV_CONT -> permanece

### 7.5 Desde ST_ERROR
- Cualquier evento -> ST_ERROR

---

## 8. Acciones de salida por transicion

- irInit()
  - noTone(PIN_BUZZER)
  - digitalWrite(PIN_MOTOR_VIBRADOR, LOW)

- irDetectando()
  - noTone(PIN_BUZZER)
  - digitalWrite(PIN_MOTOR_VIBRADOR, LOW)

- irAlertaLeve()
  - tone(PIN_BUZZER, NOTA_MI, 140)
  - digitalWrite(PIN_MOTOR_VIBRADOR, LOW)

- irAlertaFuerte()
  - Secuencia de notas (DO -> SOL -> DO_ALTO)
  - digitalWrite(PIN_MOTOR_VIBRADOR, HIGH)

- irError()
  - tone de error
  - motor vibrador encendido

---

## 9. Generacion de eventos por estado

- En ST_Init:
  - siempre genera EV_Dummy

- En ST_Detectando:
  - sinManos -> EV_Sin_manos
  - unaMano -> EV_Una_sola_mano
  - maniobraBrusca -> EV_Maniobra_sinuosa_brusca
  - maniobraLeve -> EV_Maniobra_sinuosa_leve
  - caso contrario -> EV_CONT

- En ST_AlertaLeve:
  - sinManos -> EV_Sin_manos
  - maniobraBrusca -> EV_Maniobra_sinuosa_brusca
  - tiempo en estado >= UMBRAL_TIMEOUT_ALERTA -> EV_Timeout
  - caso contrario -> EV_CONT

- En ST_AlertaFuerte:
  - tiempo en estado >= UMBRAL_TIMEOUT_ALERTA -> EV_Timeout
  - caso contrario -> EV_CONT

---

## 10. Relacion con el diagrama draw.io
La implementacion refleja el nuevo diagrama solicitado:

- Estado inicial explicito ST_Init
- Estado ST_Detectando como monitoreo base
- Estado unico ST_AlertaLeve
- Estado unico ST_AlertaFuerte
- Eventos EV_Dummy y EV_Timeout incorporados
- Salidas de actuadores acordes al estado (buzzer/motor)

Nota: la vuelta a ST_Detectando desde alertas se realiza por EV_Timeout, tal como se codifico en la tabla de transicion actual.

---

## 11. Depuracion por serial
Si SERIAL_DEBUG_ENABLED = 1, se imprime:

- Estado actual
- Evento detectado
- Separadores de lectura

Esto permite seguir en tiempo real la evolucion de la FSM.

---

## 12. Calibracion rapida

1. Ajustar UMBRAL_MANO segun valores reales de FSR.
2. Ajustar UMBRAL_MOVIMIENTO_LEVE para sensibilidad de movimiento suave.
3. Ajustar UMBRAL_MOVIMIENTO_BRUSCO para disparo de alerta fuerte.
4. Ajustar UMBRAL_DIFERENCIA_TIMEOUT para frecuencia de reaccion.
5. Ajustar UMBRAL_TIMEOUT_ALERTA para el tiempo de retorno a ST_Detectando.

Recomendacion: calibrar mirando valores por Serial y moviendo los potenciometros en Wokwi.

---

## 13. Pruebas funcionales sugeridas

1. Encendido del sistema: ST_Init debe pasar a ST_Detectando por EV_Dummy.
2. Una mano detectada: ST_Detectando -> ST_AlertaLeve.
3. Maniobra leve: ST_Detectando -> ST_AlertaLeve.
4. Sin manos: ST_Detectando -> ST_AlertaFuerte.
5. Maniobra brusca: ST_Detectando -> ST_AlertaFuerte.
6. En ST_AlertaLeve, esperar timeout: debe volver a ST_Detectando.
7. En ST_AlertaFuerte, esperar timeout: debe volver a ST_Detectando.
8. Verificar que el motor vibrador solo tenga LOW/HIGH (sin PWM).

---

## 14. Archivo principal
Toda la logica esta en:

- sketch.ino
