#include <unity.h>

#include "alarm.h"
#include "input.h"
#include "system_state.h"


/* ============================================================
 * TEMPERATURE ALARM TESTS
 * ============================================================ */

void test_temperature_below_lower_limit(void)
{
    AlarmState result =
        evaluateTemperature(10.0f);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AlarmState::LOW_TEMPERATURE),
        static_cast<int>(result)
    );
}


void test_temperature_exact_lower_limit(void)
{
    AlarmState result =
        evaluateTemperature(18.0f);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AlarmState::NORMAL),
        static_cast<int>(result)
    );
}


void test_temperature_normal_value(void)
{
    AlarmState result =
        evaluateTemperature(25.0f);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AlarmState::NORMAL),
        static_cast<int>(result)
    );
}


void test_temperature_exact_upper_limit(void)
{
    AlarmState result =
        evaluateTemperature(30.0f);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AlarmState::NORMAL),
        static_cast<int>(result)
    );
}


void test_temperature_above_upper_limit(void)
{
    AlarmState result =
        evaluateTemperature(40.0f);

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(AlarmState::HIGH_TEMPERATURE),
        static_cast<int>(result)
    );
}


/* ============================================================
 * DISPLAY NAVIGATION TESTS
 * ============================================================ */

void test_navigation_forward(void)
{
    DisplayMode result =
        nextDisplayMode(
            DisplayMode::TEMPERATURE
        );

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DisplayMode::HUMIDITY),
        static_cast<int>(result)
    );
}


void test_navigation_forward_wraparound(void)
{
    DisplayMode result =
        nextDisplayMode(
            DisplayMode::MOTION
        );

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DisplayMode::TEMPERATURE),
        static_cast<int>(result)
    );
}


void test_navigation_reverse(void)
{
    DisplayMode result =
        previousDisplayMode(
            DisplayMode::LIGHT
        );

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DisplayMode::HUMIDITY),
        static_cast<int>(result)
    );
}


void test_navigation_reverse_wraparound(void)
{
    DisplayMode result =
        previousDisplayMode(
            DisplayMode::TEMPERATURE
        );

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(DisplayMode::MOTION),
        static_cast<int>(result)
    );
}


/* ============================================================
 * SYSTEM STATE TESTS
 * ============================================================ */

void test_active_without_timeout(void)
{
    SystemState result =
        evaluateSystemState(
            SystemState::ACTIVE,
            false,
            5000
        );

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SystemState::ACTIVE),
        static_cast<int>(result)
    );
}


void test_active_with_timeout(void)
{
    SystemState result =
        evaluateSystemState(
            SystemState::ACTIVE,
            false,
            15000
        );

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SystemState::INACTIVE),
        static_cast<int>(result)
    );
}


void test_inactive_without_motion(void)
{
    SystemState result =
        evaluateSystemState(
            SystemState::INACTIVE,
            false,
            20000
        );

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SystemState::INACTIVE),
        static_cast<int>(result)
    );
}


void test_inactive_with_motion(void)
{
    SystemState result =
        evaluateSystemState(
            SystemState::INACTIVE,
            true,
            20000
        );

    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(SystemState::ACTIVE),
        static_cast<int>(result)
    );
}


/* ============================================================
 * UNITY SETUP
 * ============================================================ */

void setUp(void)
{
}


void tearDown(void)
{
}


int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();

    RUN_TEST(test_temperature_below_lower_limit);
    RUN_TEST(test_temperature_exact_lower_limit);
    RUN_TEST(test_temperature_normal_value);
    RUN_TEST(test_temperature_exact_upper_limit);
    RUN_TEST(test_temperature_above_upper_limit);

    RUN_TEST(test_navigation_forward);
    RUN_TEST(test_navigation_forward_wraparound);
    RUN_TEST(test_navigation_reverse);
    RUN_TEST(test_navigation_reverse_wraparound);

    RUN_TEST(test_active_without_timeout);
    RUN_TEST(test_active_with_timeout);
    RUN_TEST(test_inactive_without_motion);
    RUN_TEST(test_inactive_with_motion);

    return UNITY_END();
}