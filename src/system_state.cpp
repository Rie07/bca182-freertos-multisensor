#include "system_state.h"

static constexpr uint32_t INACTIVITY_TIMEOUT_MS = 15000;

SystemState evaluateSystemState(
    SystemState currentState,
    bool motionDetected,
    uint32_t elapsedWithoutMotionMs
)
{
    if (currentState == SystemState::INACTIVE)
    {
        if (motionDetected)
        {
            return SystemState::ACTIVE;
        }

        return SystemState::INACTIVE;
    }

    if (motionDetected)
    {
        return SystemState::ACTIVE;
    }

    if (elapsedWithoutMotionMs >= INACTIVITY_TIMEOUT_MS)
    {
        return SystemState::INACTIVE;
    }

    return SystemState::ACTIVE;
}