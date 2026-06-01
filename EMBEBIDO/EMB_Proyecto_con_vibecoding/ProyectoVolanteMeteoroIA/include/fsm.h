#pragma once

#include "app_types.h"

namespace volante {

class FsmController {
public:
    FsmController();

    void reset(uint32_t nowMs);
    SystemState state() const;
    uint32_t stateEnteredAt() const;
    SystemState process(SystemEvent event, uint32_t nowMs, const RuntimeConfig &config);

private:
    SystemState currentState_;
    uint32_t stateEnteredAtMs_;
};

}  // namespace volante
