#include "../../include/fsm.h"

namespace volante {

namespace {

struct TransitionRule {
    SystemState from;
    SystemEvent event;
    SystemState to;
};

constexpr TransitionRule kTransitionTable[] = {
    {SystemState::ST_INIT, SystemEvent::EV_CONT, SystemState::ST_DETECTANDO},
    {SystemState::ST_INIT, SystemEvent::EV_DUMMY, SystemState::ST_INIT},
    {SystemState::ST_INIT, SystemEvent::EV_ALARMA_CELULAR, SystemState::ST_ALARMA_CELULAR},
    {SystemState::ST_INIT, SystemEvent::EV_UNKNOW, SystemState::ST_ERROR},

    {SystemState::ST_DETECTANDO, SystemEvent::EV_CONT, SystemState::ST_DETECTANDO},
    {SystemState::ST_DETECTANDO, SystemEvent::EV_DUMMY, SystemState::ST_DETECTANDO},
    {SystemState::ST_DETECTANDO, SystemEvent::EV_UNA_SOLA_MANO, SystemState::ST_ALERTA_LEVE},
    {SystemState::ST_DETECTANDO, SystemEvent::EV_MANIOBRA_SINUOSA_LEVE, SystemState::ST_ALERTA_LEVE},
    {SystemState::ST_DETECTANDO, SystemEvent::EV_MANIOBRA_SINUOSA_BRUSCA, SystemState::ST_ALERTA_FUERTE},
    {SystemState::ST_DETECTANDO, SystemEvent::EV_SIN_MANOS, SystemState::ST_ALERTA_FUERTE},
    {SystemState::ST_DETECTANDO, SystemEvent::EV_ALARMA_CELULAR, SystemState::ST_ALARMA_CELULAR},
    {SystemState::ST_DETECTANDO, SystemEvent::EV_TIMEOUT, SystemState::ST_DETECTANDO},
    {SystemState::ST_DETECTANDO, SystemEvent::EV_UNKNOW, SystemState::ST_ERROR},

    {SystemState::ST_ALERTA_LEVE, SystemEvent::EV_CONT, SystemState::ST_ALERTA_LEVE},
    {SystemState::ST_ALERTA_LEVE, SystemEvent::EV_DUMMY, SystemState::ST_ALERTA_LEVE},
    {SystemState::ST_ALERTA_LEVE, SystemEvent::EV_UNA_SOLA_MANO, SystemState::ST_ALERTA_LEVE},
    {SystemState::ST_ALERTA_LEVE, SystemEvent::EV_MANIOBRA_SINUOSA_LEVE, SystemState::ST_ALERTA_LEVE},
    {SystemState::ST_ALERTA_LEVE, SystemEvent::EV_MANIOBRA_SINUOSA_BRUSCA, SystemState::ST_ALERTA_FUERTE},
    {SystemState::ST_ALERTA_LEVE, SystemEvent::EV_SIN_MANOS, SystemState::ST_ALERTA_FUERTE},
    {SystemState::ST_ALERTA_LEVE, SystemEvent::EV_ALARMA_CELULAR, SystemState::ST_ALARMA_CELULAR},
    {SystemState::ST_ALERTA_LEVE, SystemEvent::EV_TIMEOUT, SystemState::ST_DETECTANDO},
    {SystemState::ST_ALERTA_LEVE, SystemEvent::EV_UNKNOW, SystemState::ST_ERROR},

    {SystemState::ST_ALERTA_FUERTE, SystemEvent::EV_CONT, SystemState::ST_ALERTA_FUERTE},
    {SystemState::ST_ALERTA_FUERTE, SystemEvent::EV_DUMMY, SystemState::ST_ALERTA_FUERTE},
    {SystemState::ST_ALERTA_FUERTE, SystemEvent::EV_UNA_SOLA_MANO, SystemState::ST_ALERTA_FUERTE},
    {SystemState::ST_ALERTA_FUERTE, SystemEvent::EV_MANIOBRA_SINUOSA_LEVE, SystemState::ST_ALERTA_FUERTE},
    {SystemState::ST_ALERTA_FUERTE, SystemEvent::EV_MANIOBRA_SINUOSA_BRUSCA, SystemState::ST_ALERTA_FUERTE},
    {SystemState::ST_ALERTA_FUERTE, SystemEvent::EV_SIN_MANOS, SystemState::ST_ALERTA_FUERTE},
    {SystemState::ST_ALERTA_FUERTE, SystemEvent::EV_ALARMA_CELULAR, SystemState::ST_ALARMA_CELULAR},
    {SystemState::ST_ALERTA_FUERTE, SystemEvent::EV_TIMEOUT, SystemState::ST_DETECTANDO},
    {SystemState::ST_ALERTA_FUERTE, SystemEvent::EV_UNKNOW, SystemState::ST_ERROR},

    {SystemState::ST_ALARMA_CELULAR, SystemEvent::EV_CONT, SystemState::ST_ALARMA_CELULAR},
    {SystemState::ST_ALARMA_CELULAR, SystemEvent::EV_DUMMY, SystemState::ST_ALARMA_CELULAR},
    {SystemState::ST_ALARMA_CELULAR, SystemEvent::EV_ALARMA_CELULAR, SystemState::ST_ALARMA_CELULAR},
    {SystemState::ST_ALARMA_CELULAR, SystemEvent::EV_TIMEOUT, SystemState::ST_DETECTANDO},
    {SystemState::ST_ALARMA_CELULAR, SystemEvent::EV_UNKNOW, SystemState::ST_ERROR},

    {SystemState::ST_ERROR, SystemEvent::EV_CONT, SystemState::ST_DETECTANDO},
    {SystemState::ST_ERROR, SystemEvent::EV_DUMMY, SystemState::ST_ERROR},
    {SystemState::ST_ERROR, SystemEvent::EV_TIMEOUT, SystemState::ST_DETECTANDO},
    {SystemState::ST_ERROR, SystemEvent::EV_ALARMA_CELULAR, SystemState::ST_ALARMA_CELULAR},
    {SystemState::ST_ERROR, SystemEvent::EV_UNKNOW, SystemState::ST_ERROR},
};

SystemState nextState(SystemState current, SystemEvent event) {
    for (const auto &rule : kTransitionTable) {
        if (rule.from == current && rule.event == event) {
            return rule.to;
        }
    }

    if (event == SystemEvent::EV_UNKNOW) {
        return SystemState::ST_ERROR;
    }

    return current;
}

uint32_t timeoutForState(SystemState state, const RuntimeConfig &config) {
    switch (state) {
        case SystemState::ST_ALERTA_LEVE: return config.timings.alertaLeveMs;
        case SystemState::ST_ALERTA_FUERTE: return config.timings.alertaFuerteMs;
        case SystemState::ST_ALARMA_CELULAR: return config.timings.alarmaCelularMs;
        case SystemState::ST_ERROR: return config.timings.errorHoldMs;
        default: return 0;
    }
}

}  // namespace

FsmController::FsmController()
    : currentState_(SystemState::ST_INIT), stateEnteredAtMs_(0) {}

void FsmController::reset(uint32_t nowMs) {
    currentState_ = SystemState::ST_INIT;
    stateEnteredAtMs_ = nowMs;
}

SystemState FsmController::state() const {
    return currentState_;
}

uint32_t FsmController::stateEnteredAt() const {
    return stateEnteredAtMs_;
}

SystemState FsmController::process(SystemEvent event, uint32_t nowMs, const RuntimeConfig &config) {
    const uint32_t timeoutMs = timeoutForState(currentState_, config);
    const bool timeoutReached = timeoutMs > 0 && (nowMs - stateEnteredAtMs_) >= timeoutMs;
    const bool timeoutCanOverride = event != SystemEvent::EV_ALARMA_CELULAR && event != SystemEvent::EV_UNKNOW;
    if (timeoutReached && timeoutCanOverride) {
        event = SystemEvent::EV_TIMEOUT;
    }

    const SystemState next = nextState(currentState_, event);
    if (next != currentState_) {
        currentState_ = next;
        stateEnteredAtMs_ = nowMs;
    }

    return currentState_;
}

}  // namespace volante
