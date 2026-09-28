#include "app.h"

#include "display.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "alarm.h"
#include "buzzer.h"

#include <cstdio>
#include <cstdint>


/* ============================================================
 * Functions and peripherals provided by main.cpp
 * ============================================================ */

void UART_Print(const char *msg);

extern ADC_HandleTypeDef hadc1;

extern I2C_HandleTypeDef hi2c1;


/* ============================================================
 * Sensor data
 * ============================================================ */

struct SensorData
{
    float temperature;

    float humidity;

    int lightLevel;

    bool motionDetected;
};


/* ============================================================
 * Display modes
 * ============================================================ */

enum class DisplayMode
{
    TEMPERATURE,

    HUMIDITY,

    LIGHT,

    MOTION
};


/* ============================================================
 * FreeRTOS queues
 * ============================================================ */

/*
 * Length-1 queues are used because only the newest value
 * matters.
 */
static QueueHandle_t sensorToDisplayQueue = nullptr;
static QueueHandle_t sensorToAlarmQueue = nullptr;
static QueueHandle_t displayModeQueue = nullptr;

/* ============================================================
 * Rotary encoder movement
 *
 * The EXTI interrupt only modifies this counter.
 *
 * IMPORTANT:
 * We intentionally do NOT call FreeRTOS APIs from the encoder
 * interrupt because the Wokwi compatibility port does not
 * require an ISR context switch here.
 * ============================================================ */

static volatile int32_t encoderDelta =
    0;


/* ============================================================
 * Rotary encoder interrupt callback
 *
 * PA4 = CLK
 * PA5 = DT
 * ============================================================ */

extern "C" void HAL_GPIO_EXTI_Callback(
    uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_4)
    {
        GPIO_PinState dt =
            HAL_GPIO_ReadPin(
                GPIOA,
                GPIO_PIN_5
            );


        /*
         * Determine direction using DT when CLK falls.
         *
         * If direction appears reversed in Wokwi,
         * simply swap ++ and -- below.
         */
        if (dt == GPIO_PIN_SET)
        {
            encoderDelta++;
        }
        else
        {
            encoderDelta--;
        }
    }
}


/* ============================================================
 * InputTask
 *
 * Reads movement captured by EXTI4.
 *
 * Priority = 3
 * ============================================================ */

static void InputTask(void *pvParameters)
{
    (void)pvParameters;


    DisplayMode currentMode =
        DisplayMode::TEMPERATURE;


    UART_Print(
        "InputTask started\r\n"
    );


    /*
     * Send initial page.
     */
    xQueueOverwrite(
        displayModeQueue,
        &currentMode
    );


    for (;;)
    {
        int32_t movement;


        /*
         * Copy and clear encoder movement atomically.
         */
        taskENTER_CRITICAL();

        movement =
            encoderDelta;

        encoderDelta =
            0;

        taskEXIT_CRITICAL();


        /*
         * Process clockwise movement.
         */
        while (movement > 0)
        {
            switch (currentMode)
            {
                case DisplayMode::TEMPERATURE:

                    currentMode =
                        DisplayMode::HUMIDITY;

                    break;


                case DisplayMode::HUMIDITY:

                    currentMode =
                        DisplayMode::LIGHT;

                    break;


                case DisplayMode::LIGHT:

                    currentMode =
                        DisplayMode::MOTION;

                    break;


                case DisplayMode::MOTION:

                    currentMode =
                        DisplayMode::TEMPERATURE;

                    break;
            }


            UART_Print(
                "Encoder: CLOCKWISE\r\n"
            );


            movement--;


            /*
             * Queue only stores the newest selected page.
             */
            xQueueOverwrite(
                displayModeQueue,
                &currentMode
            );
        }


        /*
         * Process counterclockwise movement.
         */
        while (movement < 0)
        {
            switch (currentMode)
            {
                case DisplayMode::TEMPERATURE:

                    currentMode =
                        DisplayMode::MOTION;

                    break;


                case DisplayMode::HUMIDITY:

                    currentMode =
                        DisplayMode::TEMPERATURE;

                    break;


                case DisplayMode::LIGHT:

                    currentMode =
                        DisplayMode::HUMIDITY;

                    break;


                case DisplayMode::MOTION:

                    currentMode =
                        DisplayMode::LIGHT;

                    break;
            }


            UART_Print(
                "Encoder: COUNTERCLOCKWISE\r\n"
            );


            movement++;


            xQueueOverwrite(
                displayModeQueue,
                &currentMode
            );
        }


        /*
         * IMPORTANT:
         *
         * RTOS tick = 50 ms.
         *
         * This delay really blocks InputTask for one tick,
         * allowing SensorTask and DisplayTask to run.
         *
         * Encoder pulses are not lost because EXTI captures
         * them while this task is blocked.
         */
        vTaskDelay(
            pdMS_TO_TICKS(50)
        );
    }
}


/* ============================================================
 * DisplayTask
 *
 * Sole owner of the OLED.
 *
 * Priority = 1
 * ============================================================ */

static void DisplayTask(void *pvParameters)
{
    (void)pvParameters;


    SensorData data = {};


    DisplayMode currentMode =
        DisplayMode::TEMPERATURE;


    bool haveSensorData =
        false;


    UART_Print(
        "DisplayTask started\r\n"
    );


    Display_Init(
        &hi2c1
    );


    UART_Print(
        "OLED INITIALIZED\r\n"
    );


    for (;;)
    {
        bool redraw =
            false;


        /* ====================================================
         * Check for encoder page change
         * ==================================================== */

        DisplayMode newMode;


        if (
            xQueueReceive(
                displayModeQueue,
                &newMode,
                0
            ) == pdPASS
        )
        {
            if (newMode != currentMode)
            {
                currentMode =
                    newMode;


                redraw =
                    true;


                switch (currentMode)
                {
                    case DisplayMode::TEMPERATURE:

                        UART_Print(
                            "Display page: TEMPERATURE\r\n"
                        );

                        break;


                    case DisplayMode::HUMIDITY:

                        UART_Print(
                            "Display page: HUMIDITY\r\n"
                        );

                        break;


                    case DisplayMode::LIGHT:

                        UART_Print(
                            "Display page: LIGHT\r\n"
                        );

                        break;


                    case DisplayMode::MOTION:

                        UART_Print(
                            "Display page: MOTION\r\n"
                        );

                        break;
                }
            }
        }


        /* ====================================================
         * Check for new sensor data
         *
         * Wait only one RTOS tick (50 ms), NOT forever.
         *
         * This allows the display to respond to encoder
         * changes even when no new sensor reading arrives.
         * ==================================================== */

        SensorData newData;


        if (
            xQueueReceive(
                sensorToDisplayQueue,
                &newData,
                pdMS_TO_TICKS(50)
            ) == pdPASS
        )
        {
            data =
                newData;


            haveSensorData =
                true;


            redraw =
                true;


            UART_Print(
                "DisplayTask: DATA RECEIVED\r\n"
            );
        }


        /*
         * We need at least one sensor measurement before
         * showing values.
         */
        if (!haveSensorData)
        {
            continue;
        }


        /* ====================================================
         * Refresh OLED when either:
         *
         * 1. New sensor data arrived, OR
         * 2. Encoder selected another page.
         * ==================================================== */

        if (redraw)
        {
            switch (currentMode)
            {
                case DisplayMode::TEMPERATURE:

                    Display_ShowTemperature(
                        data.temperature
                    );

                    break;


                case DisplayMode::HUMIDITY:

                    Display_ShowHumidity(
                        data.humidity
                    );

                    break;


                case DisplayMode::LIGHT:

                    Display_ShowLight(
                        data.lightLevel
                    );

                    break;


                case DisplayMode::MOTION:

                    Display_ShowMotion(
                        data.motionDetected
                    );

                    break;
            }
        }
    }
}


/* ============================================================
 * DHT22
 *
 * PA1
 * ============================================================ */

#define DHT22_PORT GPIOA

#define DHT22_PIN GPIO_PIN_1


/* ============================================================
 * DWT microsecond timing
 * ============================================================ */

static void DWT_Delay_Init(void)
{
    CoreDebug->DEMCR |=
        CoreDebug_DEMCR_TRCENA_Msk;


    DWT->CTRL |=
        DWT_CTRL_CYCCNTENA_Msk;


    DWT->CYCCNT =
        0;
}


static void delay_us(uint32_t us)
{
    uint32_t start =
        DWT->CYCCNT;


    uint32_t cycles =
        us *
        (
            HAL_RCC_GetHCLKFreq() /
            1000000U
        );


    while (
        (DWT->CYCCNT - start) <
        cycles
    )
    {
    }
}


/* ============================================================
 * DHT22 GPIO output mode
 * ============================================================ */

static void DHT22_SetOutput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {};


    GPIO_InitStruct.Pin =
        DHT22_PIN;


    GPIO_InitStruct.Mode =
        GPIO_MODE_OUTPUT_PP;


    GPIO_InitStruct.Pull =
        GPIO_NOPULL;


    GPIO_InitStruct.Speed =
        GPIO_SPEED_FREQ_HIGH;


    HAL_GPIO_Init(
        DHT22_PORT,
        &GPIO_InitStruct
    );
}


/* ============================================================
 * DHT22 GPIO input mode
 * ============================================================ */

static void DHT22_SetInput(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {};


    GPIO_InitStruct.Pin =
        DHT22_PIN;


    GPIO_InitStruct.Mode =
        GPIO_MODE_INPUT;


    GPIO_InitStruct.Pull =
        GPIO_PULLUP;


    HAL_GPIO_Init(
        DHT22_PORT,
        &GPIO_InitStruct
    );
}


/* ============================================================
 * Read one DHT22 data bit
 * ============================================================ */

static uint8_t DHT22_ReadBit(void)
{
    uint32_t timeout =
        0;


    /*
     * Wait for previous HIGH pulse to finish.
     */
    while (
        HAL_GPIO_ReadPin(
            DHT22_PORT,
            DHT22_PIN
        ) == GPIO_PIN_SET
    )
    {
        if (++timeout > 1000)
        {
            return 0;
        }
    }


    timeout =
        0;


    /*
     * Wait for beginning of HIGH data pulse.
     */
    while (
        HAL_GPIO_ReadPin(
            DHT22_PORT,
            DHT22_PIN
        ) == GPIO_PIN_RESET
    )
    {
        if (++timeout > 1000)
        {
            return 0;
        }
    }


    /*
     * Sample after 40 us.
     */
    delay_us(40);


    if (
        HAL_GPIO_ReadPin(
            DHT22_PORT,
            DHT22_PIN
        ) == GPIO_PIN_SET
    )
    {
        return 1;
    }


    return 0;
}


/* ============================================================
 * Read complete DHT22 packet
 * ============================================================ */

static bool DHT22_Read(
    float *temperature,
    float *humidity)
{
    uint8_t data[5] =
    {
        0,
        0,
        0,
        0,
        0
    };


    DHT22_SetOutput();


    HAL_GPIO_WritePin(
        DHT22_PORT,
        DHT22_PIN,
        GPIO_PIN_RESET
    );


    /*
     * Start signal.
     */
    delay_us(2000);


    HAL_GPIO_WritePin(
        DHT22_PORT,
        DHT22_PIN,
        GPIO_PIN_SET
    );


    delay_us(30);


    DHT22_SetInput();


    uint32_t timeout =
        0;


    /*
     * Wait for sensor response LOW.
     */
    while (
        HAL_GPIO_ReadPin(
            DHT22_PORT,
            DHT22_PIN
        ) == GPIO_PIN_SET
    )
    {
        if (++timeout > 1000)
        {
            return false;
        }
    }


    timeout =
        0;


    /*
     * Sensor response LOW.
     */
    while (
        HAL_GPIO_ReadPin(
            DHT22_PORT,
            DHT22_PIN
        ) == GPIO_PIN_RESET
    )
    {
        if (++timeout > 1000)
        {
            return false;
        }
    }


    timeout =
        0;


    /*
     * Sensor response HIGH.
     */
    while (
        HAL_GPIO_ReadPin(
            DHT22_PORT,
            DHT22_PIN
        ) == GPIO_PIN_SET
    )
    {
        if (++timeout > 1000)
        {
            return false;
        }
    }


    /*
     * Read 40 bits.
     */
    for (int byte = 0; byte < 5; byte++)
    {
        for (int bit = 0; bit < 8; bit++)
        {
            data[byte] <<= 1;


            data[byte] |=
                DHT22_ReadBit();
        }
    }


    /*
     * Verify checksum.
     */
    uint8_t checksum =
        static_cast<uint8_t>(
            data[0] +
            data[1] +
            data[2] +
            data[3]
        );


    if (checksum != data[4])
    {
        return false;
    }


    uint16_t rawHumidity =
        (
            static_cast<uint16_t>(
                data[0]
            ) << 8
        )
        |
        data[1];


    uint16_t rawTemperature =
        (
            static_cast<uint16_t>(
                data[2]
            ) << 8
        )
        |
        data[3];


    *humidity =
        rawHumidity /
        10.0f;


    /*
     * Temperature sign bit.
     */
    if (rawTemperature & 0x8000)
    {
        rawTemperature &=
            0x7FFF;


        *temperature =
            -(
                rawTemperature /
                10.0f
            );
    }
    else
    {
        *temperature =
            rawTemperature /
            10.0f;
    }


    return true;
}


/* ============================================================
 * LDR / ADC
 * ============================================================ */

static uint16_t LDR_ReadRaw(void)
{
    HAL_ADC_Start(
        &hadc1
    );


    if (
        HAL_ADC_PollForConversion(
            &hadc1,
            100
        ) != HAL_OK
    )
    {
        HAL_ADC_Stop(
            &hadc1
        );


        return 0;
    }


    uint16_t value =
        HAL_ADC_GetValue(
            &hadc1
        );


    HAL_ADC_Stop(
        &hadc1
    );


    return value;
}


/* ============================================================
 * SensorTask
 *
 * Priority = 2
 *
 * Period = 2 seconds
 * ============================================================ */

static void SensorTask(void *pvParameters)
{
    (void)pvParameters;


    float temperature =
        0.0f;


    float humidity =
        0.0f;


    TickType_t lastWakeTime =
        xTaskGetTickCount();


    UART_Print(
        "SensorTask started\r\n"
    );


    for (;;)
    {
        UART_Print(
            "SensorTask: READING\r\n"
        );


        if (
            DHT22_Read(
                &temperature,
                &humidity
            )
        )
        {
            uint16_t lightRaw =
                LDR_ReadRaw();


            uint32_t lightPercent =
                (
                    static_cast<uint32_t>(
                        lightRaw
                    )
                    *
                    100U
                )
                /
                4095U;


            SensorData data;


            data.temperature =
                temperature;


            data.humidity =
                humidity;


            data.lightLevel =
                static_cast<int>(
                    lightPercent
                );


            /*
             * PIR comes later in the laboratory.
             */
            data.motionDetected =
                false;


            /*
             * Queue length is 1.
             *
             * Replace old sensor reading with newest reading.
             */
            xQueueOverwrite(
    sensorToDisplayQueue,
    &data
);

xQueueOverwrite(
    sensorToAlarmQueue,
    &data
);


            UART_Print(
                "Sensor data sent to queue\r\n"
            );


            /*
             * Serial output.
             */
            char message[120];


            int tempWhole =
                static_cast<int>(
                    temperature
                );


            int tempDecimal =
                static_cast<int>(
                    (
                        temperature -
                        tempWhole
                    )
                    *
                    10.0f
                );


            if (tempDecimal < 0)
            {
                tempDecimal =
                    -tempDecimal;
            }


            int humidityWhole =
                static_cast<int>(
                    humidity
                );


            int humidityDecimal =
                static_cast<int>(
                    (
                        humidity -
                        humidityWhole
                    )
                    *
                    10.0f
                );


            if (humidityDecimal < 0)
            {
                humidityDecimal =
                    -humidityDecimal;
            }


            snprintf(
                message,
                sizeof(message),
                "Temperature: %d.%d C | Humidity: %d.%d %% | Light: %d %% (ADC: %u)\r\n",
                tempWhole,
                tempDecimal,
                humidityWhole,
                humidityDecimal,
                data.lightLevel,
                lightRaw
            );


            UART_Print(
                message
            );
        }
        else
        {
            UART_Print(
                "DHT22 read failed\r\n"
            );
        }


        UART_Print(
            "SensorTask: DELAYING\r\n"
        );


        /*
         * Laboratory periodic sampling.
         */
        vTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(2000)
        );


        UART_Print(
            "SensorTask: WOKE UP\r\n"
        );
    }
}

static void AlarmTask(void *pvParameters)
{
    (void)pvParameters;

    SensorData data;

    AlarmState previousState =
        AlarmState::NORMAL;

    UART_Print(
        "AlarmTask started\r\n"
    );

    Buzzer_Off();


    for (;;)
    {
        /*
         * Wait for a new sensor reading.
         */
        if (
            xQueueReceive(
                sensorToAlarmQueue,
                &data,
                portMAX_DELAY
            ) == pdPASS
        )
        {
            AlarmState state =
                evaluateTemperature(
                    data.temperature
                );


            if (state == AlarmState::NORMAL)
            {
                Buzzer_Off();

                if (state != previousState)
                {
                    UART_Print(
                        "Alarm state: NORMAL\r\n"
                    );
                }
            }


            else if (
                state ==
                AlarmState::LOW_TEMPERATURE
            )
            {
                Buzzer_On();

                if (state != previousState)
                {
                    UART_Print(
                        "Alarm state: LOW_TEMPERATURE\r\n"
                    );
                }
            }


            else if (
                state ==
                AlarmState::HIGH_TEMPERATURE
            )
            {
                Buzzer_On();

                if (state != previousState)
                {
                    UART_Print(
                        "Alarm state: HIGH_TEMPERATURE\r\n"
                    );
                }
            }


            previousState =
                state;
        }
    }
}


/* ============================================================
 * FreeRTOS application
 * ============================================================ */

void app_main(void)
{
    /* ========================================================
     * Initialize timing and buzzer
     * ======================================================== */

    DWT_Delay_Init();

    Buzzer_Init();


    /* ========================================================
     * SENSOR -> DISPLAY QUEUE
     *
     * Length 1 = newest sensor value only.
     * ======================================================== */

    sensorToDisplayQueue =
        xQueueCreate(
            1,
            sizeof(SensorData)
        );


    if (sensorToDisplayQueue == nullptr)
    {
        UART_Print(
            "Display sensor queue creation FAILED\r\n"
        );

        for (;;)
        {
        }
    }


    /* ========================================================
     * SENSOR -> ALARM QUEUE
     *
     * AlarmTask needs its own copy of SensorData.
     * ======================================================== */

    sensorToAlarmQueue =
        xQueueCreate(
            1,
            sizeof(SensorData)
        );


    if (sensorToAlarmQueue == nullptr)
    {
        UART_Print(
            "Alarm sensor queue creation FAILED\r\n"
        );

        for (;;)
        {
        }
    }


    UART_Print(
        "SENSOR QUEUES CREATED\r\n"
    );


    /* ========================================================
     * DISPLAY MODE QUEUE
     *
     * InputTask -> DisplayTask
     *
     * Length 1 because only the newest selected page matters.
     * ======================================================== */

    displayModeQueue =
        xQueueCreate(
            1,
            sizeof(DisplayMode)
        );


    if (displayModeQueue == nullptr)
    {
        UART_Print(
            "DISPLAY MODE QUEUE CREATION FAILED\r\n"
        );

        for (;;)
        {
        }
    }


    UART_Print(
        "DISPLAY MODE QUEUE CREATED\r\n"
    );


    /* ========================================================
     * SensorTask
     *
     * Priority 2
     * ======================================================== */

    BaseType_t sensorResult =
        xTaskCreate(
            SensorTask,
            "SensorTask",
            256,
            nullptr,
            2,
            nullptr
        );


    if (sensorResult != pdPASS)
    {
        UART_Print(
            "SensorTask creation FAILED\r\n"
        );

        for (;;)
        {
        }
    }


    UART_Print(
        "SENSOR TASK CREATED\r\n"
    );


    /* ========================================================
     * AlarmTask
     *
     * Priority 2
     * ======================================================== */

    BaseType_t alarmResult =
        xTaskCreate(
            AlarmTask,
            "AlarmTask",
            256,
            nullptr,
            2,
            nullptr
        );


    if (alarmResult != pdPASS)
    {
        UART_Print(
            "AlarmTask creation FAILED\r\n"
        );

        for (;;)
        {
        }
    }


    UART_Print(
        "ALARM TASK CREATED\r\n"
    );


    /* ========================================================
     * DisplayTask
     *
     * Priority 1
     * ======================================================== */

    BaseType_t displayResult =
        xTaskCreate(
            DisplayTask,
            "DisplayTask",
            256,
            nullptr,
            1,
            nullptr
        );


    if (displayResult != pdPASS)
    {
        UART_Print(
            "DisplayTask creation FAILED\r\n"
        );

        for (;;)
        {
        }
    }


    UART_Print(
        "DISPLAY TASK CREATED\r\n"
    );


    /* ========================================================
     * InputTask
     *
     * Priority 3
     * ======================================================== */

    BaseType_t inputResult =
        xTaskCreate(
            InputTask,
            "InputTask",
            256,
            nullptr,
            3,
            nullptr
        );


    if (inputResult != pdPASS)
    {
        UART_Print(
            "InputTask creation FAILED\r\n"
        );

        for (;;)
        {
        }
    }


    UART_Print(
        "INPUT TASK CREATED\r\n"
    );


    /* ========================================================
     * Start FreeRTOS scheduler
     * ======================================================== */

    UART_Print(
        "BEFORE SCHEDULER\r\n"
    );


    vTaskStartScheduler();


    /*
     * Should never reach here.
     */
    UART_Print(
        "ERROR: SCHEDULER RETURNED\r\n"
    );


    for (;;)
    {
    }
}