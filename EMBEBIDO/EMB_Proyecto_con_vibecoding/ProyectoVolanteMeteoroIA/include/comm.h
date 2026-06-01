#pragma once

#include <WiFi.h>
#include <PubSubClient.h>

#include "app_types.h"

namespace volante {

using CommandHandler = void (*)(void *userData, const char *topic, const char *payload);

class CommService {
public:
    CommService();

    void begin(const RuntimeConfig &config);
    void setCommandHandler(CommandHandler handler, void *userData);
    void tick(uint32_t nowMs, const RuntimeConfig &config);

    bool publishState(const SensorSnapshot &snapshot, SystemState state, SystemEvent event, uint32_t nowMs, const RuntimeConfig &config);
    bool publishSensors(const SensorSnapshot &snapshot, uint32_t nowMs, const RuntimeConfig &config);

    bool wifiConnected() const;
    bool mqttConnected();

private:
    static void mqttCallback(char *topic, byte *payload, unsigned int length);

    void onMqttMessage(char *topic, byte *payload, unsigned int length);
    void ensureWifi(uint32_t nowMs, const RuntimeConfig &config);
    void ensureMqtt(uint32_t nowMs, const RuntimeConfig &config);
    bool connectMqtt(const RuntimeConfig &config);
    void setupClient(const RuntimeConfig &config);
    String buildStatePayload(const SensorSnapshot &snapshot, SystemState state, SystemEvent event, uint32_t nowMs) const;
    String buildSensorPayload(const SensorSnapshot &snapshot, uint32_t nowMs) const;

    WiFiClient wifiClient_;
    PubSubClient mqttClient_;
    CommandHandler handler_;
    void *handlerUserData_;
    uint32_t lastWifiAttemptMs_;
    uint32_t lastMqttAttemptMs_;
    static CommService *activeInstance_;
};

}  // namespace volante
