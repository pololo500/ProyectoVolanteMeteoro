#include "../../include/actuators.h"

#include <Arduino.h>

#include "../../include/app_config.h"

namespace volante {

namespace {

bool isBuzzerActive(BuzzerMode mode, bool buzzerLevel) {
    switch (mode) {
        case BuzzerMode::CONTINUOUS: return true;
        case BuzzerMode::INTERMITTENT: return buzzerLevel;
        case BuzzerMode::OFF:
        default: return false;
    }
}

}  // namespace

ActuatorService::ActuatorService()
    : activeProfile_(), buzzerLevel_(false), lastToggleMs_(0) {}

void ActuatorService::begin() {
    pinMode(config::PIN_BUZZER, OUTPUT);
    pinMode(config::PIN_MOTOR_VIBRADOR, OUTPUT);
    writeOutputs(false, false);
    activeProfile_ = OutputProfile{};
    buzzerLevel_ = false;
    lastToggleMs_ = millis();
}

void ActuatorService::writeOutputs(bool buzzerOn, bool motorOn) {
    digitalWrite(config::PIN_BUZZER, buzzerOn ? HIGH : LOW);
    digitalWrite(config::PIN_MOTOR_VIBRADOR, motorOn ? HIGH : LOW);
}

void ActuatorService::setProfile(const OutputProfile &profile, uint32_t nowMs) {
    if (profile.buzzerMode != activeProfile_.buzzerMode || profile.motorEnabled != activeProfile_.motorEnabled) {
        activeProfile_ = profile;
        buzzerLevel_ = false;
        lastToggleMs_ = nowMs;
        writeOutputs(false, activeProfile_.motorEnabled);
    }
}

void ActuatorService::tick(uint32_t nowMs, const TimingConfig &timings) {
    bool buzzerOn = false;
    if (activeProfile_.buzzerMode == BuzzerMode::CONTINUOUS) {
        buzzerOn = true;
    } else if (activeProfile_.buzzerMode == BuzzerMode::INTERMITTENT) {
        if ((nowMs - lastToggleMs_) >= timings.buzzerBlinkMs) {
            buzzerLevel_ = !buzzerLevel_;
            lastToggleMs_ = nowMs;
        }
        buzzerOn = buzzerLevel_;
    }

    writeOutputs(isBuzzerActive(activeProfile_.buzzerMode, buzzerOn), activeProfile_.motorEnabled);
}

}  // namespace volante
