#pragma once

#include "app_types.h"

namespace volante {

class ActuatorService {
public:
    ActuatorService();

    void begin();
    void setProfile(const OutputProfile &profile, uint32_t nowMs);
    void tick(uint32_t nowMs, const TimingConfig &timings);

private:
    void writeOutputs(bool buzzerOn, bool motorOn);

    OutputProfile activeProfile_;
    bool buzzerLevel_;
    uint32_t lastToggleMs_;
};

}  // namespace volante
