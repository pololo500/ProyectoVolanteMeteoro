#pragma once

#include <Arduino.h>

namespace volante {

enum class SystemState : uint8_t {
    ST_INIT = 0,
    ST_DETECTANDO,
    ST_ALERTA_LEVE,
    ST_ALERTA_FUERTE,
    ST_ALARMA_CELULAR,
    ST_ERROR,
};

enum class SystemEvent : uint8_t {
    EV_CONT = 0,
    EV_DUMMY,
    EV_UNA_SOLA_MANO,
    EV_MANIOBRA_SINUOSA_LEVE,
    EV_MANIOBRA_SINUOSA_BRUSCA,
    EV_SIN_MANOS,
    EV_ALARMA_CELULAR,
    EV_TIMEOUT,
    EV_UNKNOW,
};

enum class BuzzerMode : uint8_t {
    OFF = 0,
    INTERMITTENT,
    CONTINUOUS,
};

enum class HandPresence : uint8_t {
    NONE = 0,
    LEFT_ONLY,
    RIGHT_ONLY,
    BOTH,
};

enum class MotionType : uint8_t {
    NONE = 0,
    LEVE,
    BRUSCA,
};

struct ThresholdConfig {
    int hand = 1366;
    int movementLight = 80;
    int movementBrusque = 260;
};

struct TimingConfig {
    uint32_t singleHandMinMs = 700;
    uint32_t alertaLeveMs = 2500;
    uint32_t alertaFuerteMs = 4000;
    uint32_t alarmaCelularMs = 5000;
    uint32_t errorHoldMs = 3000;
    uint32_t eventDebounceMs = 150;
    uint32_t sensorPublishMs = 500;
    uint32_t wifiReconnectMs = 5000;
    uint32_t mqttReconnectMs = 3000;
    uint32_t buzzerBlinkMs = 250;
};

struct NetworkConfig {
    const char *ssid = "SPEEDYMONICA";
    const char *password = "#Pr0f3s5i0n4l!";
    const char *mqttHost = "broker.emqx.io";
    uint16_t mqttPort = 1883;
    const char *clientId = "esp32_volante_01";
};

struct TopicConfig {
    const char *stateTopic = "volante/estado";
    const char *sensorTopic = "volante/sensores";
    const char *commandTopic = "volante/comandos";
};

struct RuntimeConfig {
    ThresholdConfig thresholds;
    TimingConfig timings;
    NetworkConfig network;
    TopicConfig topics;
};

struct SensorSnapshot {
    int fsrLeftRaw = 0;
    int fsrRightRaw = 0;
    int steeringRaw = 0;
    int steeringDelta = 0;
    bool leftPressed = false;
    bool rightPressed = false;
    HandPresence handPresence = HandPresence::NONE;
    MotionType motion = MotionType::NONE;
    uint32_t timestampMs = 0;
};

struct SensorFrame {
    SensorSnapshot snapshot;
    SystemEvent event = SystemEvent::EV_CONT;
};

struct OutputProfile {
    BuzzerMode buzzerMode = BuzzerMode::OFF;
    bool motorEnabled = false;
};

inline const char *toString(SystemState state) {
    switch (state) {
        case SystemState::ST_INIT: return "ST_INIT";
        case SystemState::ST_DETECTANDO: return "ST_DETECTANDO";
        case SystemState::ST_ALERTA_LEVE: return "ST_ALERTA_LEVE";
        case SystemState::ST_ALERTA_FUERTE: return "ST_ALERTA_FUERTE";
        case SystemState::ST_ALARMA_CELULAR: return "ST_ALARMA_CELULAR";
        case SystemState::ST_ERROR: return "ST_ERROR";
        default: return "ST_UNKNOWN";
    }
}

inline const char *toString(SystemEvent event) {
    switch (event) {
        case SystemEvent::EV_CONT: return "EV_CONT";
        case SystemEvent::EV_DUMMY: return "EV_DUMMY";
        case SystemEvent::EV_UNA_SOLA_MANO: return "EV_UNA_SOLA_MANO";
        case SystemEvent::EV_MANIOBRA_SINUOSA_LEVE: return "EV_MANIOBRA_SINUOSA_LEVE";
        case SystemEvent::EV_MANIOBRA_SINUOSA_BRUSCA: return "EV_MANIOBRA_SINUOSA_BRUSCA";
        case SystemEvent::EV_SIN_MANOS: return "EV_SIN_MANOS";
        case SystemEvent::EV_ALARMA_CELULAR: return "EV_ALARMA_CELULAR";
        case SystemEvent::EV_TIMEOUT: return "EV_TIMEOUT";
        case SystemEvent::EV_UNKNOW: return "EV_UNKNOW";
        default: return "EV_UNKNOWN";
    }
}

inline const char *toString(BuzzerMode mode) {
    switch (mode) {
        case BuzzerMode::OFF: return "OFF";
        case BuzzerMode::INTERMITTENT: return "INTERMITTENT";
        case BuzzerMode::CONTINUOUS: return "CONTINUOUS";
        default: return "OFF";
    }
}

inline const char *toString(HandPresence presence) {
    switch (presence) {
        case HandPresence::NONE: return "NONE";
        case HandPresence::LEFT_ONLY: return "LEFT_ONLY";
        case HandPresence::RIGHT_ONLY: return "RIGHT_ONLY";
        case HandPresence::BOTH: return "BOTH";
        default: return "NONE";
    }
}

inline const char *toString(MotionType motion) {
    switch (motion) {
        case MotionType::NONE: return "NONE";
        case MotionType::LEVE: return "LEVE";
        case MotionType::BRUSCA: return "BRUSCA";
        default: return "NONE";
    }
}

inline OutputProfile profileForState(SystemState state) {
    switch (state) {
        case SystemState::ST_ALERTA_LEVE:
            return {BuzzerMode::INTERMITTENT, false};
        case SystemState::ST_ALERTA_FUERTE:
        case SystemState::ST_ALARMA_CELULAR:
            return {BuzzerMode::CONTINUOUS, true};
        case SystemState::ST_ERROR:
            return {BuzzerMode::INTERMITTENT, false};
        case SystemState::ST_INIT:
        case SystemState::ST_DETECTANDO:
        default:
            return {BuzzerMode::OFF, false};
    }
}

}  // namespace volante
