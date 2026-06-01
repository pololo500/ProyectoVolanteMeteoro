#pragma once

#include <Arduino.h>

#include "actuators.h"
#include "app_config.h"
#include "comm.h"
#include "fsm.h"
#include "sensors.h"

namespace volante {

class AppController {
public:
    AppController();

    void begin();

    RuntimeConfig copyConfig();
    SensorSnapshot copySnapshot();
    SystemState copyState();
    OutputProfile copyOutput();

    void requestExternalEvent(SystemEvent event);
    void updateThresholdHand(int value);
    void updateThresholdLight(int value);
    void updateThresholdBrusque(int value);

private:
    struct SharedData {
        RuntimeConfig config;
        SensorSnapshot snapshot;
        SystemState state;
        SystemEvent lastEvent;
        OutputProfile output;
        SystemEvent pendingExternalEvent;
        bool hasPendingExternalEvent;
    };

    static void sensorTaskThunk(void *parameter);
    static void fsmTaskThunk(void *parameter);
    static void actuatorTaskThunk(void *parameter);
    static void commTaskThunk(void *parameter);
    static void mqttCommandThunk(void *userData, const char *topic, const char *payload);

    void sensorTask();
    void fsmTask();
    void actuatorTask();
    void commTask();
    void handleMqttCommand(const char *topic, const char *payload);

    void lock();
    void unlock();

    SemaphoreHandle_t mutex_;
    SharedData shared_;
    SensorService sensorService_;
    FsmController fsmController_;
    ActuatorService actuatorService_;
    CommService commService_;
    TaskHandle_t sensorTaskHandle_;
    TaskHandle_t fsmTaskHandle_;
    TaskHandle_t actuatorTaskHandle_;
    TaskHandle_t commTaskHandle_;
};

}  // namespace volante
