#include "app.h"
#include "display.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <cstdio>
#include <cstdint>

void UART_Print(const char *msg);

struct SensorData {
    float temperature;
    float humidity;
    int lightLevel;
    bool motionDetected;
};

enum class DisplayMode
{
    TEMPERATURE,
    HUMIDITY,
    LIGHT,
    MOTION
};

static QueueHandle_t sensorQueue = nullptr;
static QueueHandle_t displayModeQueue = nullptr;

/* UART function provided by main.cpp */
void UART_Print(const char *msg);
extern ADC_HandleTypeDef hadc1;
extern I2C_HandleTypeDef hi2c1;


/* --------------------------------------------------------------------------
 * Input Task
 *
 * Reads the rotary encoder and changes the selected display page.
 *
 * Clockwise:
 * Temperature -> Humidity -> Light -> Motion -> Temperature
 *
 * Counterclockwise:
 * Temperature -> Motion -> Light -> Humidity -> Temperature
 * -------------------------------------------------------------------------- */
static void InputTask(void *pvParameters)
{
    (void)pvParameters;

    GPIO_PinState lastA =
        HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2);

    DisplayMode currentMode = DisplayMode::TEMPERATURE;

    UART_Print("InputTask started\r\n");

    for (;;)
    {
        GPIO_PinState currentA =
            HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2);

        if (currentA != lastA)
        {
            GPIO_PinState currentB =
                HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3);

            if (currentA == GPIO_PIN_RESET)
            {
                if (currentB == GPIO_PIN_SET)
                {
                    /* Clockwise */
                    switch (currentMode)
                    {
                        case DisplayMode::TEMPERATURE:
                            currentMode = DisplayMode::HUMIDITY;
                            break;

                        case DisplayMode::HUMIDITY:
                            currentMode = DisplayMode::LIGHT;
                            break;

                        case DisplayMode::LIGHT:
                            currentMode = DisplayMode::MOTION;
                            break;

                        case DisplayMode::MOTION:
                            currentMode = DisplayMode::TEMPERATURE;
                            break;
                    }

                    UART_Print("Encoder: CLOCKWISE\r\n");
                }
                else
                {
                    /* Counterclockwise */
                    switch (currentMode)
                    {
                        case DisplayMode::TEMPERATURE:
                            currentMode = DisplayMode::MOTION;
                            break;

                        case DisplayMode::HUMIDITY:
                            currentMode = DisplayMode::TEMPERATURE;
                            break;

                        case DisplayMode::LIGHT:
                            currentMode = DisplayMode::HUMIDITY;
                            break;

                        case DisplayMode::MOTION:
                            currentMode = DisplayMode::LIGHT;
                            break;
                    }

                    UART_Print("Encoder: COUNTERCLOCKWISE\r\n");
                }

                /* Send the newly selected page to DisplayTask */
                xQueueSend(
                    displayModeQueue,
                    &currentMode,
                    0
                );
            }

            lastA = currentA;
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}


/* --------------------------------------------------------------------------
 * Display Task
 *
 * Receives sensor data and displays the measurement selected
 * by the rotary encoder.
 * -------------------------------------------------------------------------- */
static void DisplayTask(void *pvParameters)
{
    (void)pvParameters;

    SensorData data;

    DisplayMode currentMode = DisplayMode::TEMPERATURE;

    UART_Print("DisplayTask started\r\n");

    Display_Init(&hi2c1);

    UART_Print("OLED INITIALIZED\r\n");

    for (;;)
    {
        /*
         * Check if the encoder selected a new page.
         * Do not block here because sensor data is also needed.
         */
        DisplayMode newMode;

        if (xQueueReceive(
                displayModeQueue,
                &newMode,
                0) == pdPASS)
        {
            currentMode = newMode;
        }

        /*
         * Wait for new sensor data.
         */
        if (xQueueReceive(
                sensorQueue,
                &data,
                portMAX_DELAY) == pdPASS)
        {
            UART_Print("DisplayTask: DATA RECEIVED\r\n");

            switch (currentMode)
            {
                case DisplayMode::TEMPERATURE:
                    Display_ShowTemperature(data.temperature);
                    break;

                case DisplayMode::HUMIDITY:
                    Display_ShowHumidity(data.humidity);
                    break;

                case DisplayMode::LIGHT:
                    Display_ShowLight(data.lightLevel);
                    break;

                case DisplayMode::MOTION:
                    Display_ShowMotion(data.motionDetected);
                    break;
            }
        }
    }
}


/* DHT22 is connected to PA1 */
#define DHT22_PORT GPIOA
#define DHT22_PIN  GPIO_PIN_1


/* --------------------------------------------------------------------------
 * Microsecond delay using the Cortex-M3 DWT cycle counter
 * -------------------------------------------------------------------------- */
static void DWT_Delay_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    DWT->CYCCNT = 0;
}


static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles =
        us * (HAL_RCC_GetHCLKFreq() / 1000000U);

    while ((DWT->CYCCNT - start) < cycles)
    {
        /* Wait */
    }
}


/* --------------------------------------------------------------------------
 * Change PA1 to output mode
 * -------------------------------------------------------------------------- */
static void DHT22_SetOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {};

    GPIO_InitStruct.Pin = DHT22_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(DHT22_PORT, &GPIO_InitStruct);
}


/* --------------------------------------------------------------------------
 * Change PA1 to input mode
 * -------------------------------------------------------------------------- */
static void DHT22_SetInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {};

    GPIO_InitStruct.Pin = DHT22_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;

    HAL_GPIO_Init(DHT22_PORT, &GPIO_InitStruct);
}


/* --------------------------------------------------------------------------
 * Read one DHT22 bit
 * -------------------------------------------------------------------------- */
static uint8_t DHT22_ReadBit(void)
{
    uint32_t timeout = 0;

    while (HAL_GPIO_ReadPin(DHT22_PORT, DHT22_PIN) == GPIO_PIN_SET)
    {
        if (++timeout > 1000)
            return 0;
    }

    timeout = 0;

    while (HAL_GPIO_ReadPin(DHT22_PORT, DHT22_PIN) == GPIO_PIN_RESET)
    {
        if (++timeout > 1000)
            return 0;
    }

    delay_us(40);

    if (HAL_GPIO_ReadPin(DHT22_PORT, DHT22_PIN) == GPIO_PIN_SET)
    {
        return 1;
    }

    return 0;
}


/* --------------------------------------------------------------------------
 * Read the complete 40-bit DHT22 packet
 * -------------------------------------------------------------------------- */
static bool DHT22_Read(float *temperature, float *humidity)
{
    uint8_t data[5] = {0};

    DHT22_SetOutput();

    HAL_GPIO_WritePin(
        DHT22_PORT,
        DHT22_PIN,
        GPIO_PIN_RESET
    );

    delay_us(2000);

    HAL_GPIO_WritePin(
        DHT22_PORT,
        DHT22_PIN,
        GPIO_PIN_SET
    );

    delay_us(30);

    DHT22_SetInput();

    uint32_t timeout = 0;

    while (HAL_GPIO_ReadPin(DHT22_PORT, DHT22_PIN) == GPIO_PIN_SET)
    {
        if (++timeout > 1000)
            return false;
    }

    timeout = 0;

    while (HAL_GPIO_ReadPin(DHT22_PORT, DHT22_PIN) == GPIO_PIN_RESET)
    {
        if (++timeout > 1000)
            return false;
    }

    timeout = 0;

    while (HAL_GPIO_ReadPin(DHT22_PORT, DHT22_PIN) == GPIO_PIN_SET)
    {
        if (++timeout > 1000)
            return false;
    }

    for (int byte = 0; byte < 5; byte++)
    {
        for (int bit = 0; bit < 8; bit++)
        {
            data[byte] <<= 1;
            data[byte] |= DHT22_ReadBit();
        }
    }

    uint8_t checksum =
        (uint8_t)(data[0] + data[1] + data[2] + data[3]);

    if (checksum != data[4])
    {
        return false;
    }

    uint16_t rawHumidity =
        ((uint16_t)data[0] << 8) | data[1];

    uint16_t rawTemperature =
        ((uint16_t)data[2] << 8) | data[3];

    *humidity = rawHumidity / 10.0f;

    if (rawTemperature & 0x8000)
    {
        rawTemperature &= 0x7FFF;
        *temperature = -(rawTemperature / 10.0f);
    }
    else
    {
        *temperature = rawTemperature / 10.0f;
    }

    return true;
}


static uint16_t LDR_ReadRaw(void)
{
    HAL_ADC_Start(&hadc1);

    if (HAL_ADC_PollForConversion(&hadc1, 100) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        return 0;
    }

    uint16_t value = HAL_ADC_GetValue(&hadc1);

    HAL_ADC_Stop(&hadc1);

    return value;
}


/* --------------------------------------------------------------------------
 * Sensor Task
 * -------------------------------------------------------------------------- */
static void SensorTask(void *pvParameters)
{
    (void)pvParameters;

    float temperature = 0.0f;
    float humidity = 0.0f;

    TickType_t lastWakeTime =
        xTaskGetTickCount();

    UART_Print("SensorTask started\r\n");

    for (;;)
    {
        UART_Print("SensorTask: READING\r\n");

        if (DHT22_Read(&temperature, &humidity))
        {
            uint16_t lightRaw = LDR_ReadRaw();

            uint32_t lightPercent =
                ((uint32_t)lightRaw * 100U) / 4095U;

            SensorData data;

            data.temperature = temperature;
            data.humidity = humidity;
            data.lightLevel = (int)lightPercent;
            data.motionDetected = false;

            if (xQueueSend(sensorQueue, &data, 0) == pdPASS)
            {
                UART_Print("Sensor data sent to queue\r\n");
            }
            else
            {
                UART_Print("Sensor queue full\r\n");
            }

            char message[120];

            int temp_whole = (int)temperature;
            int temp_decimal =
                (int)((temperature - temp_whole) * 10);

            int humidity_whole = (int)humidity;
            int humidity_decimal =
                (int)((humidity - humidity_whole) * 10);

            snprintf(
                message,
                sizeof(message),
                "Temperature: %d.%d C | Humidity: %d.%d %% | Light: %d %% (ADC: %u)\r\n",
                temp_whole,
                temp_decimal,
                humidity_whole,
                humidity_decimal,
                data.lightLevel,
                lightRaw
            );

            UART_Print(message);
        }
        else
        {
            UART_Print("DHT22 read failed\r\n");
        }

        UART_Print("SensorTask: DELAYING\r\n");

        vTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(2000)
        );

        UART_Print("SensorTask: WOKE UP\r\n");
    }
}


/* --------------------------------------------------------------------------
 * FreeRTOS application entry point
 * -------------------------------------------------------------------------- */
void app_main(void)
{
    DWT_Delay_Init();

    sensorQueue =
        xQueueCreate(5, sizeof(SensorData));

    if (sensorQueue == nullptr)
    {
        UART_Print("Sensor queue creation FAILED\r\n");
        for (;;) {}
    }

    UART_Print("SENSOR QUEUE CREATED\r\n");

    /*
     * Queue for rotary encoder display selection.
     */
    displayModeQueue =
        xQueueCreate(5, sizeof(DisplayMode));

    if (displayModeQueue == nullptr)
    {
        UART_Print("DISPLAY MODE QUEUE CREATION FAILED\r\n");
        for (;;) {}
    }

    UART_Print("DISPLAY MODE QUEUE CREATED\r\n");


    /* Create SensorTask */
    BaseType_t sensorResult = xTaskCreate(
        SensorTask,
        "SensorTask",
        256,
        nullptr,
        2,
        nullptr
    );

    if (sensorResult != pdPASS)
    {
        UART_Print("SensorTask creation FAILED\r\n");
        for (;;) {}
    }

    UART_Print("SENSOR TASK CREATED\r\n");


    /* Create DisplayTask */
    BaseType_t displayResult = xTaskCreate(
        DisplayTask,
        "DisplayTask",
        256,
        nullptr,
        1,
        nullptr
    );

    if (displayResult != pdPASS)
    {
        UART_Print("DisplayTask creation FAILED\r\n");
        for (;;) {}
    }

    UART_Print("DISPLAY TASK CREATED\r\n");


    /* Create InputTask */
    BaseType_t inputResult = xTaskCreate(
        InputTask,
        "InputTask",
        256,
        nullptr,
        1,
        nullptr
    );

    if (inputResult != pdPASS)
    {
        UART_Print("InputTask creation FAILED\r\n");
        for (;;) {}
    }

    UART_Print("INPUT TASK CREATED\r\n");

    UART_Print("BEFORE SCHEDULER\r\n");

    vTaskStartScheduler();

    UART_Print("SCHEDULER RETURNED\r\n");

    for (;;) {}
}