#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <stdbool.h>
#include <stdint.h>

enum class SystemState
{
    ACTIVE,
    INACTIVE
};

SystemState evaluateSystemState(
    SystemState currentState,
    bool motionDetected,
    uint32_t elapsedWithoutMotionMs
);

#endif