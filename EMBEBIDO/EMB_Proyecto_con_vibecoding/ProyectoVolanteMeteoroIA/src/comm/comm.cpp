#include "../../include/comm.h"

#include <Arduino.h>

namespace volante {

CommService *CommService::activeInstance_ = nullptr;

namespace {

String escapeJsonText(const char *value) {
    return String(value ? value : "");
}

}  // namespace

CommService::CommService()
    : mqttClient_(wifiClient_), handler_(nullptr), handlerUserData_(nullptr), lastWifiAttemptMs_(0), lastMqttAttemptMs_(0) {}

void CommService::begin(const RuntimeConfig &config) {
    activeInstance_ = this;
    setupClient(config);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
    lastWifiAttemptMs_ = 0;
    lastMqttAttemptMs_ = 0;
}

void CommService::setupClient(const RuntimeConfig &config) {
    mqttClient_.setServer(config.network.mqttHost, config.network.mqttPort);
    mqttClient_.setCallback(CommService::mqttCallback);
    mqttClient_.setKeepAlive(30);
    mqttClient_.setSocketTimeout(3);
    mqttClient_.setBufferSize(512);
}

void CommService::setCommandHandler(CommandHandler handler, void *userData) {
    handler_ = handler;
    handlerUserData_ = userData;
}

bool CommService::wifiConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

bool CommService::mqttConnected() {
    return mqttClient_.connected();
}

void CommService::mqttCallback(char *topic, byte *payload, unsigned int length) {
    if (activeInstance_ != nullptr) {
        activeInstance_->onMqttMessage(topic, payload, length);
    }
}

void CommService::onMqttMessage(char *topic, byte *payload, unsigned int length) {
    if (handler_ == nullptr) {
        return;
    }

    static char buffer[256];
    const unsigned int copyLength = length < sizeof(buffer) - 1 ? length : sizeof(buffer) - 1;
    memcpy(buffer, payload, copyLength);
    buffer[copyLength] = '\0';
    handler_(handlerUserData_, topic, buffer);
}

void CommService::ensureWifi(uint32_t nowMs, const RuntimeConfig &config) {
    if (wifiConnected()) {
        return;
    }

    if ((nowMs - lastWifiAttemptMs_) < config.timings.wifiReconnectMs) {
        return;
    }

    lastWifiAttemptMs_ = nowMs;
    WiFi.disconnect(false);
    WiFi.begin(config.network.ssid, config.network.password);
}

bool CommService::connectMqtt(const RuntimeConfig &config) {
    if (!wifiConnected()) {
        return false;
    }

    const bool connected = mqttClient_.connect(config.network.clientId);
    if (connected) {
        mqttClient_.subscribe(config.topics.commandTopic);
    }
    return connected;
}

void CommService::ensureMqtt(uint32_t nowMs, const RuntimeConfig &config) {
    if (!wifiConnected()) {
        return;
    }

    if (mqttConnected()) {
        return;
    }

    if ((nowMs - lastMqttAttemptMs_) < config.timings.mqttReconnectMs) {
        return;
    }

    lastMqttAttemptMs_ = nowMs;
    connectMqtt(config);
}

void CommService::tick(uint32_t nowMs, const RuntimeConfig &config) {
    ensureWifi(nowMs, config);

    if (wifiConnected() && !mqttConnected()) {
        ensureMqtt(nowMs, config);
    }

    if (mqttConnected()) {
        mqttClient_.loop();
    }
}

String CommService::buildStatePayload(const SensorSnapshot &snapshot, SystemState state, SystemEvent event, uint32_t nowMs) const {
    char payload[384];
    snprintf(payload, sizeof(payload),
             "{\"estado\":\"%s\",\"evento\":\"%s\",\"timestamp_ms\":%lu,\"fsr_izq\":%d,\"fsr_der\":%d,\"volante\":%d,\"delta_volante\":%d,\"manos\":\"%s\",\"movimiento\":\"%s\"}",
             toString(state),
             toString(event),
             static_cast<unsigned long>(nowMs),
             snapshot.fsrLeftRaw,
             snapshot.fsrRightRaw,
             snapshot.steeringRaw,
             snapshot.steeringDelta,
             toString(snapshot.handPresence),
             toString(snapshot.motion));
    return String(payload);
}

String CommService::buildSensorPayload(const SensorSnapshot &snapshot, uint32_t nowMs) const {
    char payload[384];
    snprintf(payload, sizeof(payload),
             "{\"timestamp_ms\":%lu,\"fsr_izq\":%d,\"fsr_der\":%d,\"volante\":%d,\"delta_volante\":%d,\"manos\":\"%s\",\"movimiento\":\"%s\"}",
             static_cast<unsigned long>(nowMs),
             snapshot.fsrLeftRaw,
             snapshot.fsrRightRaw,
             snapshot.steeringRaw,
             snapshot.steeringDelta,
             toString(snapshot.handPresence),
             toString(snapshot.motion));
    return String(payload);
}

bool CommService::publishState(const SensorSnapshot &snapshot, SystemState state, SystemEvent event, uint32_t nowMs, const RuntimeConfig &config) {
    if (!mqttConnected()) {
        return false;
    }

    const String payload = buildStatePayload(snapshot, state, event, nowMs);
    return mqttClient_.publish(config.topics.stateTopic, payload.c_str(), true);
}

bool CommService::publishSensors(const SensorSnapshot &snapshot, uint32_t nowMs, const RuntimeConfig &config) {
    if (!mqttConnected()) {
        return false;
    }

    const String payload = buildSensorPayload(snapshot, nowMs);
    return mqttClient_.publish(config.topics.sensorTopic, payload.c_str(), false);
}

}  // namespace volante
