#include "../../include/sensors.h"

#include <Arduino.h>

#include "../../include/app_config.h"

namespace volante {

namespace {

MotionType classifyMotion(int delta, const ThresholdConfig &thresholds) {
    const int magnitude = abs(delta);
    if (magnitude >= thresholds.movementBrusque) {
        return MotionType::BRUSCA;
    }
    if (magnitude >= thresholds.movementLight) {
        return MotionType::LEVE;
    }
    return MotionType::NONE;
}

}  // namespace

SensorService::SensorService()
    : previousSteeringRaw_(0), currentCandidateEvent_(SystemEvent::EV_CONT), lastEmittedEvent_(SystemEvent::EV_CONT), candidateSinceMs_(0) {}

void SensorService::begin() {
    analogReadResolution(12);
    analogSetPinAttenuation(config::PIN_FSR_IZQ, ADC_11db);
    analogSetPinAttenuation(config::PIN_FSR_DER, ADC_11db);
    analogSetPinAttenuation(config::PIN_VOLANTE, ADC_11db);
    pinMode(config::PIN_FSR_IZQ, INPUT);
    pinMode(config::PIN_FSR_DER, INPUT);
    pinMode(config::PIN_VOLANTE, INPUT);

    previousSteeringRaw_ = analogRead(config::PIN_VOLANTE);
    currentCandidateEvent_ = SystemEvent::EV_CONT;
    lastEmittedEvent_ = SystemEvent::EV_CONT;
    candidateSinceMs_ = millis();
}

HandPresence SensorService::deriveHandPresence(const SensorSnapshot &snapshot) const {
    if (snapshot.leftPressed && snapshot.rightPressed) {
        return HandPresence::BOTH;
    }
    if (snapshot.leftPressed) {
        return HandPresence::LEFT_ONLY;
    }
    if (snapshot.rightPressed) {
        return HandPresence::RIGHT_ONLY;
    }
    return HandPresence::NONE;
}

SystemEvent SensorService::deriveEvent(const SensorSnapshot &snapshot, const RuntimeConfig &config, uint32_t nowMs) {
    const bool oneHandDetected = (snapshot.handPresence == HandPresence::LEFT_ONLY || snapshot.handPresence == HandPresence::RIGHT_ONLY);

    SystemEvent rawCandidate = SystemEvent::EV_CONT;
    if (snapshot.handPresence == HandPresence::NONE) {
        rawCandidate = SystemEvent::EV_SIN_MANOS;
    } else if (snapshot.motion == MotionType::BRUSCA) {
        rawCandidate = SystemEvent::EV_MANIOBRA_SINUOSA_BRUSCA;
    } else if (snapshot.motion == MotionType::LEVE) {
        rawCandidate = SystemEvent::EV_MANIOBRA_SINUOSA_LEVE;
    } else if (oneHandDetected) {
        rawCandidate = SystemEvent::EV_UNA_SOLA_MANO;
    }

    if (rawCandidate != currentCandidateEvent_) {
        currentCandidateEvent_ = rawCandidate;
        candidateSinceMs_ = nowMs;
    }

    if (currentCandidateEvent_ == SystemEvent::EV_UNA_SOLA_MANO) {
        if ((nowMs - candidateSinceMs_) < config.timings.singleHandMinMs) {
            return lastEmittedEvent_;
        }
    } else if ((nowMs - candidateSinceMs_) < config.timings.eventDebounceMs) {
        return lastEmittedEvent_;
    }

    lastEmittedEvent_ = currentCandidateEvent_;
    return lastEmittedEvent_;
}

SensorFrame SensorService::sample(uint32_t nowMs, const RuntimeConfig &config) {
    SensorFrame frame;
    frame.snapshot.timestampMs = nowMs;

    frame.snapshot.fsrLeftRaw = analogRead(config::PIN_FSR_IZQ);
    frame.snapshot.fsrRightRaw = analogRead(config::PIN_FSR_DER);
    frame.snapshot.steeringRaw = analogRead(config::PIN_VOLANTE);
    frame.snapshot.steeringDelta = frame.snapshot.steeringRaw - previousSteeringRaw_;
    previousSteeringRaw_ = frame.snapshot.steeringRaw;

    frame.snapshot.leftPressed = frame.snapshot.fsrLeftRaw >= config.thresholds.hand;
    frame.snapshot.rightPressed = frame.snapshot.fsrRightRaw >= config.thresholds.hand;
    frame.snapshot.handPresence = deriveHandPresence(frame.snapshot);
    frame.snapshot.motion = classifyMotion(frame.snapshot.steeringDelta, config.thresholds);
    frame.event = deriveEvent(frame.snapshot, config, nowMs);

    return frame;
}

}  // namespace volante
