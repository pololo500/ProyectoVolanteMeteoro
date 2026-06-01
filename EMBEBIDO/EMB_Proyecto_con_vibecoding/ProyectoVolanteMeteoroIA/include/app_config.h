#pragma once

#include "app_types.h"

namespace volante::config {

constexpr uint8_t PIN_BUZZER = 33;
constexpr uint8_t PIN_MOTOR_VIBRADOR = 27;
constexpr uint8_t PIN_FSR_IZQ = 34;
constexpr uint8_t PIN_FSR_DER = 35;
constexpr uint8_t PIN_VOLANTE = 4;

inline RuntimeConfig makeDefaultRuntimeConfig() {
    RuntimeConfig cfg;
    cfg.thresholds = ThresholdConfig{1366, 80, 260};
    cfg.timings = TimingConfig{700, 2500, 4000, 5000, 3000, 150, 500, 5000, 3000, 250};
    cfg.network = NetworkConfig{"SPEEDYMONICA", "#Pr0f3s5i0n4l!", "broker.emqx.io", 1883, "esp32_volante_01"};
    cfg.topics = TopicConfig{"volante/estado", "volante/sensores", "volante/comandos"};
    return cfg;
}

}  // namespace volante::config
