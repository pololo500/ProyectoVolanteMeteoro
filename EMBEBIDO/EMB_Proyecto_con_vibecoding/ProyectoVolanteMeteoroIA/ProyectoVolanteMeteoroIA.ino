#include "include/app_controller.h"

using namespace volante;

AppController app;

void setup() {
    app.begin();
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}