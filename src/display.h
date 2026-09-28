#ifndef DISPLAY_H
#define DISPLAY_H

#include "stm32f1xx_hal.h"

void Display_Init(I2C_HandleTypeDef *hi2c);
void Display_Clear(void);

void Display_ShowTemperature(float temperature);
void Display_ShowHumidity(float humidity);
void Display_ShowLight(int lightLevel);
void Display_ShowMotion(bool motionDetected);

#endif