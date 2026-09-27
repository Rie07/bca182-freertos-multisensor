#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

#include <cstdio>
#include <cstring>

/* ---- Peripheral handles ------------------------------------------------ */
UART_HandleTypeDef huart1;

/* ---- Forward declarations ---------------------------------------------- */
extern "C" void Error_Handler(void);
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void UART_Print(const char *msg);

static void TaskA(void *pvParameters);
static void TaskB(void *pvParameters);

static TaskHandle_t taskA_handle = nullptr;
static TaskHandle_t taskB_handle = nullptr;

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();

    UART_Print("BCA182 FreeRTOS Multisensor\r\n");
    UART_Print("System starting...\r\n");

    /* Part 17: two simple tasks, both must block between executions
     * (Part 19: no uncontrolled busy loops). */
   BaseType_t resultA = xTaskCreate(TaskA, "TaskA", configMINIMAL_STACK_SIZE, nullptr, tskIDLE_PRIORITY + 1, &taskA_handle); UART_Print(resultA == pdPASS ? "TaskA created OK\r\n" : "TaskA create FAILED\r\n"); 
   BaseType_t resultB = xTaskCreate(TaskB, "TaskB", configMINIMAL_STACK_SIZE, nullptr, tskIDLE_PRIORITY + 1, &taskB_handle); UART_Print(resultB == pdPASS ? "TaskB created OK\r\n" : "TaskB create FAILED\r\n");
   UART_Print("Starting scheduler...\r\n");
   vTaskStartScheduler();

    /* Should never reach here — if it does, heap allocation for the
     * idle/timer task failed. */
    for (;;) {
    }
}

static void TaskA(void *pvParameters)
{
    (void)pvParameters;
    for (;;) {
        UART_Print("Task A running\r\n");
        vTaskDelay(pdMS_TO_TICKS(1000)); /* Blocked here -> Running when it wakes */
    }
}

static void TaskB(void *pvParameters)
{
    (void)pvParameters;
    for (;;) {
        UART_Print("Task B running\r\n");
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

/* ---- UART helper --------------------------------------------------------
 * Not yet mutex-protected — Part XI adds a mutex once a second writer
 * actually contends for this resource. */
static void UART_Print(const char *msg)
{
    HAL_UART_Transmit(&huart1, reinterpret_cast<const uint8_t *>(msg),
                       strlen(msg), HAL_MAX_DELAY);
}

/* ---- Clock config: 72 MHz from 8 MHz HSE, standard Blue Pill setup ---- */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9; /* 8MHz * 9 = 72MHz */
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    /* Onboard PC13 LED, sensor/actuator GPIO added in later parts */
}

static void MX_USART1_UART_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}

/* HAL_UART_MspInit configures the actual GPIO pins (PA9=TX, PA10=RX)
 * for USART1. Normally CubeMX generates this for you; since this
 * project is hand-written, we provide it ourselves. */
extern "C" void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        GPIO_InitTypeDef GPIO_InitStruct = {};

        GPIO_InitStruct.Pin = GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

extern "C" void Error_Handler(void)
{
    __disable_irq();
    for (;;) {
    }
}

/* Called by FreeRTOS if pvPortMalloc() ever fails (heap exhausted).
 * Triggered because configUSE_MALLOC_FAILED_HOOK is 1. */
extern "C" void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

/* Called by FreeRTOS if it detects a task has overrun its stack.
 * Triggered because configCHECK_FOR_STACK_OVERFLOW is 2. */
extern "C" void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}