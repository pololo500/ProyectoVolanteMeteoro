#include "../../include/app_controller.h"

#include <Arduino.h>

namespace volante {

namespace {

uint32_t nowTick() {
    return millis();
}

}  // namespace

AppController::AppController()
    : mutex_(nullptr),
      shared_{config::makeDefaultRuntimeConfig(), SensorSnapshot{}, SystemState::ST_INIT, SystemEvent::EV_CONT, OutputProfile{}, SystemEvent::EV_CONT, false},
      sensorService_(),
      fsmController_(),
      actuatorService_(),
      commService_(),
      sensorTaskHandle_(nullptr),
      fsmTaskHandle_(nullptr),
      actuatorTaskHandle_(nullptr),
      commTaskHandle_(nullptr) {}

void AppController::lock() {
    if (mutex_ != nullptr) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
    }
}

void AppController::unlock() {
    if (mutex_ != nullptr) {
        xSemaphoreGive(mutex_);
    }
}

RuntimeConfig AppController::copyConfig() {
    RuntimeConfig copy;
    lock();
    copy = shared_.config;
    unlock();
    return copy;
}

SensorSnapshot AppController::copySnapshot() {
    SensorSnapshot copy;
    lock();
    copy = shared_.snapshot;
    unlock();
    return copy;
}

SystemState AppController::copyState() {
    SystemState copy = SystemState::ST_INIT;
    lock();
    copy = shared_.state;
    unlock();
    return copy;
}

OutputProfile AppController::copyOutput() {
    OutputProfile copy;
    lock();
    copy = shared_.output;
    unlock();
    return copy;
}

void AppController::requestExternalEvent(SystemEvent event) {
    lock();
    shared_.pendingExternalEvent = event;
    shared_.hasPendingExternalEvent = true;
    unlock();
}

void AppController::updateThresholdHand(int value) {
    lock();
    shared_.config.thresholds.hand = value;
    unlock();
}

void AppController::updateThresholdLight(int value) {
    lock();
    shared_.config.thresholds.movementLight = value;
    unlock();
}

void AppController::updateThresholdBrusque(int value) {
    lock();
    shared_.config.thresholds.movementBrusque = value;
    unlock();
}

void AppController::begin() {
    Serial.begin(115200);
    delay(200);

    mutex_ = xSemaphoreCreateMutex();
    shared_.config = config::makeDefaultRuntimeConfig();
    shared_.snapshot = SensorSnapshot{};
    shared_.state = SystemState::ST_INIT;
    shared_.lastEvent = SystemEvent::EV_CONT;
    shared_.output = profileForState(shared_.state);
    shared_.pendingExternalEvent = SystemEvent::EV_CONT;
    shared_.hasPendingExternalEvent = false;

    sensorService_.begin();
    actuatorService_.begin();
    commService_.begin(shared_.config);
    commService_.setCommandHandler(&AppController::mqttCommandThunk, this);

    fsmController_.reset(nowTick());
    lock();
    shared_.state = fsmController_.state();
    shared_.output = profileForState(shared_.state);
    unlock();

    xTaskCreatePinnedToCore(sensorTaskThunk, "sensorTask", 4096, this, 3, &sensorTaskHandle_, 1);
    xTaskCreatePinnedToCore(fsmTaskThunk, "fsmTask", 4096, this, 4, &fsmTaskHandle_, 1);
    xTaskCreatePinnedToCore(actuatorTaskThunk, "actuatorTask", 4096, this, 2, &actuatorTaskHandle_, 1);
    xTaskCreatePinnedToCore(commTaskThunk, "commTask", 6144, this, 2, &commTaskHandle_, 0);

    Serial.println("Firmware ESP32 volante listo.");
}

void AppController::sensorTaskThunk(void *parameter) {
    static_cast<AppController *>(parameter)->sensorTask();
}

void AppController::fsmTaskThunk(void *parameter) {
    static_cast<AppController *>(parameter)->fsmTask();
}

void AppController::actuatorTaskThunk(void *parameter) {
    static_cast<AppController *>(parameter)->actuatorTask();
}

void AppController::commTaskThunk(void *parameter) {
    static_cast<AppController *>(parameter)->commTask();
}

void AppController::mqttCommandThunk(void *userData, const char *topic, const char *payload) {
    static_cast<AppController *>(userData)->handleMqttCommand(topic, payload);
}

void AppController::sensorTask() {
    const TickType_t period = pdMS_TO_TICKS(50);
    TickType_t lastWake = xTaskGetTickCount();

    while (true) {
        const uint32_t nowMs = nowTick();
        RuntimeConfig configCopy = copyConfig();
        const SensorFrame frame = sensorService_.sample(nowMs, configCopy);

        lock();
        shared_.snapshot = frame.snapshot;
        shared_.lastEvent = frame.event;
        unlock();

        vTaskDelayUntil(&lastWake, period);
    }
}

void AppController::fsmTask() {
    const TickType_t period = pdMS_TO_TICKS(50);
    TickType_t lastWake = xTaskGetTickCount();

    while (true) {
        const uint32_t nowMs = nowTick();
        RuntimeConfig configCopy = copyConfig();
        SensorSnapshot snapshotCopy = copySnapshot();
        SystemEvent sensorEvent = SystemEvent::EV_CONT;

        SystemEvent externalEvent = SystemEvent::EV_CONT;
        lock();
        sensorEvent = shared_.lastEvent;
        if (shared_.hasPendingExternalEvent) {
            externalEvent = shared_.pendingExternalEvent;
            shared_.pendingExternalEvent = SystemEvent::EV_CONT;
            shared_.hasPendingExternalEvent = false;
        }
        unlock();

        SystemEvent eventToProcess = externalEvent != SystemEvent::EV_CONT ? externalEvent : sensorEvent;
        const SystemState previousState = copyState();
        const SystemState newState = fsmController_.process(eventToProcess, nowMs, configCopy);
        const OutputProfile output = profileForState(newState);

        lock();
        shared_.state = newState;
        shared_.output = output;
        shared_.snapshot = snapshotCopy;
        shared_.lastEvent = eventToProcess;
        unlock();

        if (newState != previousState) {
            actuatorService_.setProfile(output, nowMs);
        }

        vTaskDelayUntil(&lastWake, period);
    }
}

void AppController::actuatorTask() {
    const TickType_t period = pdMS_TO_TICKS(20);
    TickType_t lastWake = xTaskGetTickCount();

    while (true) {
        const uint32_t nowMs = nowTick();
        const OutputProfile profile = copyOutput();
        actuatorService_.setProfile(profile, nowMs);
        actuatorService_.tick(nowMs, copyConfig().timings);
        vTaskDelayUntil(&lastWake, period);
    }
}

void AppController::commTask() {
    const TickType_t period = pdMS_TO_TICKS(100);
    TickType_t lastWake = xTaskGetTickCount();
    uint32_t lastSensorPublishMs = 0;
    uint32_t lastStatePublishMs = 0;
    SystemState lastPublishedState = SystemState::ST_INIT;

    while (true) {
        const uint32_t nowMs = nowTick();
        RuntimeConfig configCopy = copyConfig();

        commService_.tick(nowMs, configCopy);

        const SensorSnapshot snapshot = copySnapshot();
        const SystemState currentState = copyState();
        const bool mqttReady = commService_.mqttConnected();

        if (mqttReady && (nowMs - lastSensorPublishMs) >= configCopy.timings.sensorPublishMs) {
            commService_.publishSensors(snapshot, nowMs, configCopy);
            lastSensorPublishMs = nowMs;
        }

        if (mqttReady && (currentState != lastPublishedState || (nowMs - lastStatePublishMs) >= configCopy.timings.sensorPublishMs)) {
            const SystemEvent event = shared_.lastEvent;
            commService_.publishState(snapshot, currentState, event, nowMs, configCopy);
            lastPublishedState = currentState;
            lastStatePublishMs = nowMs;
        }

        vTaskDelayUntil(&lastWake, period);
    }
}

void AppController::handleMqttCommand(const char *topic, const char *payload) {
    const RuntimeConfig configCopy = copyConfig();
    if (strcmp(topic, configCopy.topics.commandTopic) != 0) {
        return;
    }

    String command = String(payload ? payload : "");
    command.trim();
    command.toUpperCase();

    if (command == "ALARMA") {
        requestExternalEvent(SystemEvent::EV_ALARMA_CELULAR);
        return;
    }

    const int sepIndex = command.indexOf(':');
    if (sepIndex <= 0) {
        requestExternalEvent(SystemEvent::EV_UNKNOW);
        return;
    }

    const String key = command.substring(0, sepIndex);
    const String valueText = command.substring(sepIndex + 1);
    const int value = valueText.toInt();

    if (key == "UMBRAL_MANO") {
        updateThresholdHand(value);
    } else if (key == "UMBRAL_LEVE") {
        updateThresholdLight(value);
    } else if (key == "UMBRAL_BRUSCO") {
        updateThresholdBrusque(value);
    } else {
        requestExternalEvent(SystemEvent::EV_UNKNOW);
    }
}

}  // namespace volante
