#pragma once

#include "app_types.h"

namespace volante {

class SensorService {
public:
    SensorService();

    void begin();
    SensorFrame sample(uint32_t nowMs, const RuntimeConfig &config);

private:
    SystemEvent deriveEvent(const SensorSnapshot &snapshot, const RuntimeConfig &config, uint32_t nowMs);
    HandPresence deriveHandPresence(const SensorSnapshot &snapshot) const;

    int previousSteeringRaw_;
    SystemEvent currentCandidateEvent_;
    SystemEvent lastEmittedEvent_;
    uint32_t candidateSinceMs_;
};

}  // namespace volante
