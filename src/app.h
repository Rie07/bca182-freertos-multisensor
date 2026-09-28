#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

#include <cstdio>
#include <cstdint>

#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

void app_main(void);

#ifdef __cplusplus
}
#endif

#endif