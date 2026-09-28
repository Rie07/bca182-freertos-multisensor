#include "buzzer.h"

TIM_HandleTypeDef htim2;


/* ============================================================
 * BUZZER INITIALIZATION
 *
 * Buzzer signal = PA2
 * PA2 = TIM2 Channel 3
 * ============================================================ */

void Buzzer_Init(void)
{
    /*
     * Enable clocks.
     */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();


    /* --------------------------------------------------------
     * Configure PA2 as TIM2_CH3 PWM output
     * -------------------------------------------------------- */

    GPIO_InitTypeDef GPIO_InitStruct = {};

    GPIO_InitStruct.Pin =
        GPIO_PIN_2;

    GPIO_InitStruct.Mode =
        GPIO_MODE_AF_PP;

    GPIO_InitStruct.Speed =
        GPIO_SPEED_FREQ_HIGH;

    HAL_GPIO_Init(
        GPIOA,
        &GPIO_InitStruct
    );


    /* --------------------------------------------------------
     * Configure TIM2
     *
     * Timer clock = 72 MHz
     *
     * 72 MHz / 72 = 1 MHz
     * 1 MHz / 1000 = 1000 Hz
     *
     * Buzzer frequency = 1 kHz
     * -------------------------------------------------------- */

    htim2.Instance =
        TIM2;

    htim2.Init.Prescaler =
        71;

    htim2.Init.CounterMode =
        TIM_COUNTERMODE_UP;

    htim2.Init.Period =
        999;

    htim2.Init.ClockDivision =
        TIM_CLOCKDIVISION_DIV1;

    htim2.Init.AutoReloadPreload =
        TIM_AUTORELOAD_PRELOAD_DISABLE;


    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
    {
        return;
    }


    /* --------------------------------------------------------
     * PWM Channel 3
     * -------------------------------------------------------- */

    TIM_OC_InitTypeDef sConfigOC = {};

    sConfigOC.OCMode =
        TIM_OCMODE_PWM1;

    /*
     * 50% duty cycle
     */
    sConfigOC.Pulse =
        500;

    sConfigOC.OCPolarity =
        TIM_OCPOLARITY_HIGH;

    sConfigOC.OCFastMode =
        TIM_OCFAST_DISABLE;


    if (
        HAL_TIM_PWM_ConfigChannel(
            &htim2,
            &sConfigOC,
            TIM_CHANNEL_3
        ) != HAL_OK
    )
    {
        return;
    }


    /*
     * Start OFF.
     */
    HAL_TIM_PWM_Stop(
        &htim2,
        TIM_CHANNEL_3
    );
}


/* ============================================================
 * BUZZER ON
 * ============================================================ */

void Buzzer_On(void)
{
    /*
     * Set approximately 50% duty cycle.
     */
    __HAL_TIM_SET_COMPARE(
        &htim2,
        TIM_CHANNEL_3,
        500
    );


    HAL_TIM_PWM_Start(
        &htim2,
        TIM_CHANNEL_3
    );
}


/* ============================================================
 * BUZZER OFF
 * ============================================================ */

void Buzzer_Off(void)
{
    HAL_TIM_PWM_Stop(
        &htim2,
        TIM_CHANNEL_3
    );


    __HAL_TIM_SET_COMPARE(
        &htim2,
        TIM_CHANNEL_3,
        0
    );
}