#include "app.h"
#include "display.h"
#include "alarm.h"
#include "buzzer.h"
#include "system_state.h"
#include "input.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"

#include <cstdio>
#include <cstdint>


/* ============================================================
 * External functions / peripherals
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
 * Queues
 * ============================================================ */

static QueueHandle_t sensorToDisplayQueue =
    nullptr;

static QueueHandle_t sensorToAlarmQueue =
    nullptr;

static QueueHandle_t displayModeQueue =
    nullptr;

/* ============================================================
 * FreeRTOS Event Group
 * ============================================================ */

static EventGroupHandle_t systemEvents = nullptr;


/*
 * Bit 0:
 * System is currently ACTIVE.
 *
 * Set by StateTask.
 * Read by InputTask, DisplayTask and AlarmTask.
 */
#define EVENT_ACTIVE   (1 << 0)


/*
 * Bit 1:
 * Motion is currently detected.
 *
 * Set/cleared by MotionTask.
 * Read by StateTask.
 */
#define EVENT_MOTION   (1 << 1)


/*
 * Bit 2:
 * Temperature alarm is active.
 *
 * Set/cleared by AlarmTask.
 * Can be read by DisplayTask later.
 */
#define EVENT_ALARM    (1 << 2)

/* ============================================================
 * Shared motion / system-state variables
 * ============================================================ */

static volatile bool motionDetected =
    false;

static volatile SystemState currentSystemState =
    SystemState::ACTIVE;


/* ============================================================
 * Encoder movement
 * ============================================================ */

static volatile int32_t encoderDelta =
    0;


/* ============================================================
 * Encoder EXTI callback
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
 * ============================================================ */

static void InputTask(void *pvParameters)
{
    (void)pvParameters;

    DisplayMode currentMode =
        DisplayMode::TEMPERATURE;

    UART_Print(
        "InputTask started\r\n"
    );

    xQueueOverwrite(
        displayModeQueue,
        &currentMode
    );

    for (;;)
    {
        /*
         * Ignore encoder while system is inactive.
         */
        EventBits_t bits =
            xEventGroupGetBits(
                systemEvents
            );

        if (
            (bits & EVENT_ACTIVE) == 0
        )
        {
            vTaskDelay(
                pdMS_TO_TICKS(50)
            );

            continue;
        }


        int32_t movement;


        taskENTER_CRITICAL();

        movement =
            encoderDelta;

        encoderDelta =
            0;

        taskEXIT_CRITICAL();


        /*
         * Clockwise
         */
        while (movement > 0)
        {
            currentMode =
                nextDisplayMode(
                    currentMode
                );

            UART_Print(
                "Encoder: CLOCKWISE\r\n"
            );

            xQueueOverwrite(
                displayModeQueue,
                &currentMode
            );

            movement--;
        }


        /*
         * Counterclockwise
         */
        while (movement < 0)
        {
            currentMode =
                previousDisplayMode(
                    currentMode
                );

            UART_Print(
                "Encoder: COUNTERCLOCKWISE\r\n"
            );

            xQueueOverwrite(
                displayModeQueue,
                &currentMode
            );

            movement++;
        }


        vTaskDelay(
            pdMS_TO_TICKS(50)
        );
    }
}


/* ============================================================
 * DisplayTask
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


        /*
         * If inactive, blank the display.
         */
        EventBits_t eventBits =
    xEventGroupGetBits(
        systemEvents
    );


if (
    (eventBits & EVENT_ACTIVE) == 0
)
{
    Display_Clear();

    vTaskDelay(
        pdMS_TO_TICKS(100)
    );

    continue;
}


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


        if (!haveSensorData)
        {
            continue;
        }


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
 * MotionTask
 * ============================================================ */

static void MotionTask(void *pvParameters)
{
    (void)pvParameters;

    bool previousMotion = false;

    UART_Print(
        "MotionTask started\r\n"
    );


    for (;;)
    {
        bool currentMotion =
            (
                HAL_GPIO_ReadPin(
                    GPIOA,
                    GPIO_PIN_3
                ) == GPIO_PIN_SET
            );


        /*
         * Keep existing variable working.
         */
        motionDetected =
            currentMotion;


        /*
         * Update EVENT_MOTION.
         */
        if (currentMotion)
        {
            xEventGroupSetBits(
                systemEvents,
                EVENT_MOTION
            );
        }
        else
        {
            xEventGroupClearBits(
                systemEvents,
                EVENT_MOTION
            );
        }


        /*
         * Print only when motion begins.
         */
        if (
            currentMotion &&
            !previousMotion
        )
        {
            UART_Print(
                "Motion detected\r\n"
            );
        }


        /*
         * Print only when motion ends.
         */
        if (
            !currentMotion &&
            previousMotion
        )
        {
            UART_Print(
                "Motion stopped\r\n"
            );
        }


        previousMotion =
            currentMotion;


        vTaskDelay(
            pdMS_TO_TICKS(100)
        );
    }
}


/* ============================================================
 * StateTask
 * ============================================================ */

static void StateTask(void *pvParameters)
{
    (void)pvParameters;


    SystemState state =
        SystemState::ACTIVE;


    TickType_t lastMotionTime =
        xTaskGetTickCount();


    /*
     * Initial system state.
     */
    currentSystemState =
        SystemState::ACTIVE;


    /*
     * EVENT_ACTIVE starts SET.
     */
    xEventGroupSetBits(
        systemEvents,
        EVENT_ACTIVE
    );


    UART_Print(
        "StateTask started\r\n"
    );


    UART_Print(
        "System state: ACTIVE\r\n"
    );


    for (;;)
    {
        TickType_t now =
            xTaskGetTickCount();


        /*
         * Read EVENT_MOTION from event group.
         */
        EventBits_t bits =
            xEventGroupGetBits(
                systemEvents
            );


        bool motion =
            (
                bits &
                EVENT_MOTION
            ) != 0;


        /*
         * Motion resets inactivity timer.
         */
        if (motion)
        {
            lastMotionTime =
                now;
        }


        uint32_t elapsedMs =
            static_cast<uint32_t>(
                (now - lastMotionTime) *
                portTICK_PERIOD_MS
            );


        SystemState newState =
            evaluateSystemState(
                state,
                motion,
                elapsedMs
            );


        /*
         * Only process when state changes.
         */
        if (newState != state)
        {
            state =
                newState;


            currentSystemState =
                state;


            if (
                state ==
                SystemState::ACTIVE
            )
            {
                /*
                 * Signal ACTIVE state.
                 */
                xEventGroupSetBits(
                    systemEvents,
                    EVENT_ACTIVE
                );


                UART_Print(
                    "System state: ACTIVE\r\n"
                );
            }
            else
            {
                /*
                 * Signal INACTIVE state.
                 */
                xEventGroupClearBits(
                    systemEvents,
                    EVENT_ACTIVE
                );


                /*
                 * Silence buzzer immediately.
                 */
                Buzzer_Off();


                /*
                 * Clear alarm event.
                 */
                xEventGroupClearBits(
                    systemEvents,
                    EVENT_ALARM
                );


                /*
                 * Turn OLED off/blank.
                 */
                Display_Clear();


                UART_Print(
                    "System state: INACTIVE\r\n"
                );
            }
        }


        currentSystemState =
            state;


        vTaskDelay(
            pdMS_TO_TICKS(250)
        );
    }
}


/* ============================================================
 * DHT22
 * ============================================================ */

#define DHT22_PORT GPIOA
#define DHT22_PIN  GPIO_PIN_1


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


static uint8_t DHT22_ReadBit(void)
{
    uint32_t timeout =
        0;

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


    for (int byte = 0; byte < 5; byte++)
    {
        for (int bit = 0; bit < 8; bit++)
        {
            data[byte] <<= 1;

            data[byte] |=
                DHT22_ReadBit();
        }
    }


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
 * LDR
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
 * AlarmTask
 * ============================================================ */

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
        if (
            xQueueReceive(
                sensorToAlarmQueue,
                &data,
                portMAX_DELAY
            ) == pdPASS
        )
        {
            /*
             * Check whether system is ACTIVE
             * using the Event Group.
             */
            EventBits_t bits =
                xEventGroupGetBits(
                    systemEvents
                );


            bool systemActive =
                (
                    bits &
                    EVENT_ACTIVE
                ) != 0;


            /*
             * Alarm disabled while INACTIVE.
             */
            if (!systemActive)
            {
                Buzzer_Off();


                xEventGroupClearBits(
                    systemEvents,
                    EVENT_ALARM
                );


                continue;
            }


            AlarmState state =
                evaluateTemperature(
                    data.temperature
                );


            /* =================================================
             * NORMAL
             * ================================================= */

            if (
                state ==
                AlarmState::NORMAL
            )
            {
                Buzzer_Off();


                /*
                 * Alarm no longer active.
                 */
                xEventGroupClearBits(
                    systemEvents,
                    EVENT_ALARM
                );


                if (
                    state !=
                    previousState
                )
                {
                    UART_Print(
                        "Alarm state: NORMAL - BUZZER OFF\r\n"
                    );
                }
            }


            /* =================================================
             * LOW TEMPERATURE
             * ================================================= */

            else if (
                state ==
                AlarmState::LOW_TEMPERATURE
            )
            {
                Buzzer_On();


                /*
                 * Signal alarm.
                 */
                xEventGroupSetBits(
                    systemEvents,
                    EVENT_ALARM
                );


                if (
                    state !=
                    previousState
                )
                {
                    UART_Print(
                        "Alarm state: LOW_TEMPERATURE - BUZZER ON\r\n"
                    );
                }
            }


            /* =================================================
             * HIGH TEMPERATURE
             * ================================================= */

            else if (
                state ==
                AlarmState::HIGH_TEMPERATURE
            )
            {
                Buzzer_On();


                /*
                 * Signal alarm.
                 */
                xEventGroupSetBits(
                    systemEvents,
                    EVENT_ALARM
                );


                if (
                    state !=
                    previousState
                )
                {
                    UART_Print(
                        "Alarm state: HIGH_TEMPERATURE - BUZZER ON\r\n"
                    );
                }
            }


            previousState =
                state;
        }
    }
}


/* ============================================================
 * SensorTask
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


            data.motionDetected =
                motionDetected;


            xQueueOverwrite(
                sensorToDisplayQueue,
                &data
            );


            xQueueOverwrite(
                sensorToAlarmQueue,
                &data
            );


            UART_Print(
                "Sensor data sent to queues\r\n"
            );


            char message[150];


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
                "Temperature: %d.%d C | Humidity: %d.%d %% | Light: %d %% | Motion: %s\r\n",
                tempWhole,
                tempDecimal,
                humidityWhole,
                humidityDecimal,
                data.lightLevel,
                data.motionDetected ? "YES" : "NO"
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


        vTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(2000)
        );


        UART_Print(
            "SensorTask: WOKE UP\r\n"
        );
    }
}


/* ============================================================
 * app_main
 * ============================================================ */

void app_main(void)
{
    DWT_Delay_Init();

    Buzzer_Init();

        /* ========================================================
     * Create System Event Group
     * ======================================================== */

    systemEvents =
        xEventGroupCreate();


    if (systemEvents == nullptr)
    {
        UART_Print(
            "EVENT GROUP CREATION FAILED\r\n"
        );


        for (;;)
        {
        }
    }


    /*
     * System begins ACTIVE.
     */
    xEventGroupSetBits(
        systemEvents,
        EVENT_ACTIVE
    );


    UART_Print(
        "SYSTEM EVENT GROUP CREATED\r\n"
    );

    /* ========================================================
     * Sensor -> Display Queue
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
     * Sensor -> Alarm Queue
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
     * Display Mode Queue
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
     * SensorTask - Priority 2
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
     * AlarmTask - Priority 2
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
     * DisplayTask - Priority 1
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
     * InputTask - Priority 3
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
     * MotionTask - Priority 3
     * ======================================================== */

    BaseType_t motionResult =
        xTaskCreate(
            MotionTask,
            "MotionTask",
            256,
            nullptr,
            3,
            nullptr
        );


    if (motionResult != pdPASS)
    {
        UART_Print(
            "MotionTask creation FAILED\r\n"
        );

        for (;;)
        {
        }
    }


    UART_Print(
        "MOTION TASK CREATED\r\n"
    );


    /* ========================================================
     * StateTask - Priority 3
     * ======================================================== */

    BaseType_t stateResult =
        xTaskCreate(
            StateTask,
            "StateTask",
            256,
            nullptr,
            3,
            nullptr
        );


    if (stateResult != pdPASS)
    {
        UART_Print(
            "StateTask creation FAILED\r\n"
        );

        for (;;)
        {
        }
    }


    UART_Print(
        "STATE TASK CREATED\r\n"
    );


    /* ========================================================
     * Start scheduler
     * ======================================================== */

    UART_Print(
        "BEFORE SCHEDULER\r\n"
    );


    vTaskStartScheduler();


    UART_Print(
        "ERROR: SCHEDULER RETURNED\r\n"
    );


    for (;;)
    {
    }
}