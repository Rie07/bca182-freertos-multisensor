#include "app.h"
#include "FreeRTOS.h"
#include "task.h"

void UART_Print(const char *msg);

static void TaskA(void *pvParameters);
static void TaskB(void *pvParameters);

void app_main(void)
{
    xTaskCreate(TaskA, "TaskA", 128, nullptr, 2, nullptr);
    xTaskCreate(TaskB, "TaskB", 128, nullptr, 2, nullptr);

    UART_Print("BEFORE SCHEDULER\r\n");

    vTaskStartScheduler();

    UART_Print("SCHEDULER RETURNED\r\n");

    for (;;)
    {
    }
}

static void TaskA(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        UART_Print("Task A: RUNNING\r\n");

        UART_Print("Task A: BLOCKED for 1 second\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void TaskB(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        UART_Print("Task B: RUNNING\r\n");

        UART_Print("Task B: BLOCKED for 1.5 seconds\r\n");
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}