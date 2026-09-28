#include "stm32f1xx_hal.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "app.h"

#include <cstring>


/* ============================================================
 * Peripheral handles
 * ============================================================ */

UART_HandleTypeDef huart1;
ADC_HandleTypeDef hadc1;
I2C_HandleTypeDef hi2c1;

static SemaphoreHandle_t uartMutex = nullptr;

/* ============================================================
 * Forward declarations
 * ============================================================ */

extern "C" void Error_Handler(void);

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);

void UART_Print(const char *msg);


/* ============================================================
 * Wokwi FreeRTOS compatibility support
 * ============================================================ */

extern "C" BaseType_t xPortConsumeTickYield(void);

extern "C" void vApplicationIdleHook(void)
{
    __WFI();

    if (xPortConsumeTickYield() != pdFALSE)
    {
        taskYIELD();
    }
}


/* ============================================================
 * Main
 * ============================================================ */

int main(void)
{
    SCB->VTOR = FLASH_BASE;

    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
MX_USART1_UART_Init();
MX_ADC1_Init();
MX_I2C1_Init();

/*
 * Create UART mutex.
 */
uartMutex =
    xSemaphoreCreateMutex();

if (uartMutex == nullptr)
{
    Error_Handler();
}

UART_Print("\r\n");
    UART_Print("BCA182 FreeRTOS Multisensor\r\n");
    UART_Print("SYSTEM INITIALIZED\r\n");

    app_main();

    for (;;)
    {
    }
}


/* ============================================================
 * UART helper
 * ============================================================ */

void UART_Print(const char *msg)
{
    /*
     * Before the scheduler starts, print normally.
     *
     * After the scheduler starts, protect UART using
     * the mutex so only one task prints at a time.
     */

    if (
        uartMutex != nullptr &&
        xTaskGetSchedulerState() == taskSCHEDULER_RUNNING
    )
    {
        if (
            xSemaphoreTake(
                uartMutex,
                portMAX_DELAY
            ) == pdTRUE
        )
        {
            HAL_UART_Transmit(
                &huart1,
                reinterpret_cast<const uint8_t *>(msg),
                strlen(msg),
                HAL_MAX_DELAY
            );

            xSemaphoreGive(
                uartMutex
            );
        }
    }
    else
    {
        HAL_UART_Transmit(
            &huart1,
            reinterpret_cast<const uint8_t *>(msg),
            strlen(msg),
            HAL_MAX_DELAY
        );
    }
}


/* ============================================================
 * System Clock
 * ============================================================ */

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {};

    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSE;

    RCC_OscInitStruct.HSEState =
        RCC_HSE_ON;

    RCC_OscInitStruct.HSEPredivValue =
        RCC_HSE_PREDIV_DIV1;

    RCC_OscInitStruct.PLL.PLLState =
        RCC_PLL_ON;

    RCC_OscInitStruct.PLL.PLLSource =
        RCC_PLLSOURCE_HSE;

    RCC_OscInitStruct.PLL.PLLMUL =
        RCC_PLL_MUL9;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_PLLCLK;

    RCC_ClkInitStruct.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    RCC_ClkInitStruct.APB1CLKDivider =
        RCC_HCLK_DIV2;

    RCC_ClkInitStruct.APB2CLKDivider =
        RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(
            &RCC_ClkInitStruct,
            FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}


/* ============================================================
 * GPIO initialization
 * ============================================================ */

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {};


    /* ========================================================
     * DHT22
     *
     * PA1 = DATA
     * ======================================================== */

    GPIO_InitStruct.Pin =
        GPIO_PIN_1;

    GPIO_InitStruct.Mode =
        GPIO_MODE_OUTPUT_PP;

    GPIO_InitStruct.Pull =
        GPIO_NOPULL;

    GPIO_InitStruct.Speed =
        GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(
        GPIOA,
        &GPIO_InitStruct
    );

    HAL_GPIO_WritePin(
        GPIOA,
        GPIO_PIN_1,
        GPIO_PIN_SET
    );


    /* ========================================================
     * LDR
     *
     * PA0 = ADC1_IN0
     * ======================================================== */

    GPIO_InitStruct.Pin =
        GPIO_PIN_0;

    GPIO_InitStruct.Mode =
        GPIO_MODE_ANALOG;

    GPIO_InitStruct.Pull =
        GPIO_NOPULL;

    HAL_GPIO_Init(
        GPIOA,
        &GPIO_InitStruct
    );


    /* ========================================================
     * PIR SENSOR
     *
     * PA3 = PIR OUT
     * ======================================================== */

    GPIO_InitStruct.Pin =
        GPIO_PIN_3;

    GPIO_InitStruct.Mode =
        GPIO_MODE_INPUT;

    GPIO_InitStruct.Pull =
        GPIO_PULLDOWN;

    HAL_GPIO_Init(
        GPIOA,
        &GPIO_InitStruct
    );


    /* ========================================================
     * OLED
     *
     * PB6 = SCL
     * PB7 = SDA
     * ======================================================== */

    GPIO_InitStruct.Pin =
        GPIO_PIN_6 |
        GPIO_PIN_7;

    GPIO_InitStruct.Mode =
        GPIO_MODE_AF_OD;

    GPIO_InitStruct.Pull =
        GPIO_PULLUP;

    GPIO_InitStruct.Speed =
        GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(
        GPIOB,
        &GPIO_InitStruct
    );


    /* ========================================================
     * ROTARY ENCODER CLK
     *
     * PA4 = EXTI4
     * ======================================================== */

    GPIO_InitStruct.Pin =
        GPIO_PIN_4;

    GPIO_InitStruct.Mode =
        GPIO_MODE_IT_FALLING;

    GPIO_InitStruct.Pull =
        GPIO_PULLUP;

    HAL_GPIO_Init(
        GPIOA,
        &GPIO_InitStruct
    );


    /* ========================================================
     * ROTARY ENCODER DT
     *
     * PA5
     * ======================================================== */

    GPIO_InitStruct.Pin =
        GPIO_PIN_5;

    GPIO_InitStruct.Mode =
        GPIO_MODE_INPUT;

    GPIO_InitStruct.Pull =
        GPIO_PULLUP;

    HAL_GPIO_Init(
        GPIOA,
        &GPIO_InitStruct
    );


    /* ========================================================
     * ROTARY ENCODER BUTTON
     *
     * PB0
     * ======================================================== */

    GPIO_InitStruct.Pin =
        GPIO_PIN_0;

    GPIO_InitStruct.Mode =
        GPIO_MODE_INPUT;

    GPIO_InitStruct.Pull =
        GPIO_PULLUP;

    HAL_GPIO_Init(
        GPIOB,
        &GPIO_InitStruct
    );


    /* ========================================================
     * Enable encoder EXTI4
     * ======================================================== */

    HAL_NVIC_SetPriority(
        EXTI4_IRQn,
        6,
        0
    );

    HAL_NVIC_EnableIRQ(
        EXTI4_IRQn
    );
}


/* ============================================================
 * USART1
 * ============================================================ */

static void MX_USART1_UART_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();

    huart1.Instance =
        USART1;

    huart1.Init.BaudRate =
        115200;

    huart1.Init.WordLength =
        UART_WORDLENGTH_8B;

    huart1.Init.StopBits =
        UART_STOPBITS_1;

    huart1.Init.Parity =
        UART_PARITY_NONE;

    huart1.Init.Mode =
        UART_MODE_TX_RX;

    huart1.Init.HwFlowCtl =
        UART_HWCONTROL_NONE;

    huart1.Init.OverSampling =
        UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}


/* ============================================================
 * ADC1
 * ============================================================ */

static void MX_ADC1_Init(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();

    hadc1.Instance =
        ADC1;

    hadc1.Init.ScanConvMode =
        ADC_SCAN_DISABLE;

    hadc1.Init.ContinuousConvMode =
        DISABLE;

    hadc1.Init.DiscontinuousConvMode =
        DISABLE;

    hadc1.Init.ExternalTrigConv =
        ADC_SOFTWARE_START;

    hadc1.Init.DataAlign =
        ADC_DATAALIGN_RIGHT;

    hadc1.Init.NbrOfConversion =
        1;

    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    ADC_ChannelConfTypeDef sConfig = {};

    sConfig.Channel =
        ADC_CHANNEL_0;

    sConfig.Rank =
        ADC_REGULAR_RANK_1;

    sConfig.SamplingTime =
        ADC_SAMPLETIME_239CYCLES_5;

    if (HAL_ADC_ConfigChannel(
            &hadc1,
            &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }
}


/* ============================================================
 * I2C1
 * ============================================================ */

static void MX_I2C1_Init(void)
{
    __HAL_RCC_I2C1_CLK_ENABLE();

    hi2c1.Instance =
        I2C1;

    hi2c1.Init.ClockSpeed =
        100000;

    hi2c1.Init.DutyCycle =
        I2C_DUTYCYCLE_2;

    hi2c1.Init.OwnAddress1 =
        0;

    hi2c1.Init.AddressingMode =
        I2C_ADDRESSINGMODE_7BIT;

    hi2c1.Init.DualAddressMode =
        I2C_DUALADDRESS_DISABLE;

    hi2c1.Init.OwnAddress2 =
        0;

    hi2c1.Init.GeneralCallMode =
        I2C_GENERALCALL_DISABLE;

    hi2c1.Init.NoStretchMode =
        I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
        Error_Handler();
    }
}


/* ============================================================
 * UART MSP
 * ============================================================ */

extern "C" void HAL_UART_MspInit(
    UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        GPIO_InitTypeDef GPIO_InitStruct = {};

        GPIO_InitStruct.Pin =
            GPIO_PIN_9;

        GPIO_InitStruct.Mode =
            GPIO_MODE_AF_PP;

        GPIO_InitStruct.Speed =
            GPIO_SPEED_FREQ_HIGH;

        HAL_GPIO_Init(
            GPIOA,
            &GPIO_InitStruct
        );

        GPIO_InitStruct.Pin =
            GPIO_PIN_10;

        GPIO_InitStruct.Mode =
            GPIO_MODE_INPUT;

        GPIO_InitStruct.Pull =
            GPIO_NOPULL;

        HAL_GPIO_Init(
            GPIOA,
            &GPIO_InitStruct
        );
    }
}


/* ============================================================
 * Encoder interrupt
 * ============================================================ */

extern "C" void EXTI4_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(
        GPIO_PIN_4
    );
}


/* ============================================================
 * Error Handler
 * ============================================================ */

extern "C" void Error_Handler(void)
{
    __disable_irq();

    for (;;)
    {
    }
}


/* ============================================================
 * FreeRTOS hooks
 * ============================================================ */

extern "C" void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();

    for (;;)
    {
    }
}


extern "C" void vApplicationStackOverflowHook(
    TaskHandle_t xTask,
    char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    taskDISABLE_INTERRUPTS();

    for (;;)
    {
    }
}


/* ============================================================
 * SysTick
 * ============================================================ */

extern "C" void xPortSysTickHandler(void);

extern "C" void SysTick_Handler(void)
{
    HAL_IncTick();

    if (
        xTaskGetSchedulerState() !=
        taskSCHEDULER_NOT_STARTED
    )
    {
        xPortSysTickHandler();
    }
}