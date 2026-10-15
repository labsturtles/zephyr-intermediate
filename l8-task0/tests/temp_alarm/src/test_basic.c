
/*
 * temp_alarm - Happy-path unit tests
 *
 * Module 8 - Task 1
 *
 * Tests:
 * - Initial status
 * - Temperature below threshold
 * - Temperature at threshold
 * - Temperature above threshold
 * - Threshold changes
 * - Alarm reset
 */

#include <errno.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include "sensor_fake.h"
#include "temp_alarm.h"

#define DEFAULT_THRESHOLD 30

/*
 * Reset the sensor fake and initialize the module
 * before every test.
 */
static void before(void *fixture)
{
    ARG_UNUSED(fixture);

    sensor_fake_reset();
    sensor_fake_init();

    zassume_ok(
        temp_alarm_init(&fake_sensor_dev, DEFAULT_THRESHOLD),
        "precondition: temp_alarm_init must succeed"
    );
}

/*
 * ============================================================
 * Suite: temp_alarm_get_status
 * ============================================================
 */

ZTEST_SUITE(temp_alarm_get_status, NULL, NULL, before, NULL, NULL);

ZTEST(temp_alarm_get_status, test_status_after_init)
{
    struct temp_alarm_status status;

    zassert_ok(temp_alarm_get_status(&status),
               "get_status must succeed");

    zassert_equal(status.last_temp, 0,
                  "last_temp must be 0 after init");

    zassert_equal(status.alarm_count, 0U,
                  "alarm_count must be 0 after init");

    zassert_false(status.is_alarming,
                  "is_alarming must be false after init");

    zassert_equal(status.last_error, 0,
                  "last_error must be 0 after init");
}

/*
 * ============================================================
 * Suite: temp_alarm_check
 * ============================================================
 */

ZTEST_SUITE(temp_alarm_check, NULL, NULL, before, NULL, NULL);

/*
 * Test 1: Temperature below threshold.
 *
 * Threshold: 30 C
 * Temperature: 20 C
 * Expected: No alarm.
 */
ZTEST(temp_alarm_check, test_below_threshold)
{
    struct temp_alarm_status status;

    sensor_fake_set_temperature(20);

    zassert_ok(temp_alarm_check(),
               "check must succeed");

    zassert_ok(temp_alarm_get_status(&status),
               "get_status must succeed");

    zassert_equal(status.last_temp, 20,
                  "last_temp must be 20");

    zassert_false(status.is_alarming,
                  "alarm must be inactive");

    zassert_equal(fake_sensor_sample_fetch_fake.call_count, 1,
                  "sample_fetch must be called once");

    zassert_equal(fake_sensor_channel_get_fake.call_count, 1,
                  "channel_get must be called once");

    zassert_equal(fake_sensor_channel_get_fake.arg1_val,
                  SENSOR_CHAN_AMBIENT_TEMP,
                  "incorrect sensor channel");
}

/*
 * Test 2: Temperature exactly at threshold.
 *
 * Threshold: 30 C
 * Temperature: 30 C
 * Expected: No alarm.
 */
ZTEST(temp_alarm_check, test_at_threshold)
{
    struct temp_alarm_status status;

    sensor_fake_set_temperature(30);

    zassert_ok(temp_alarm_check(),
               "check must succeed");

    zassert_ok(temp_alarm_get_status(&status),
               "get_status must succeed");

    zassert_false(status.is_alarming,
                  "alarm must remain inactive");

    zassert_equal(status.alarm_count, 0U,
                  "alarm_count must remain zero");
}

/*
 * Test 3: Temperature above threshold.
 *
 * Threshold: 30 C
 * Temperature: 40 C
 * Expected: Alarm active, count = 1.
 */
ZTEST(temp_alarm_check, test_above_threshold)
{
    struct temp_alarm_status status;

    sensor_fake_set_temperature(40);

    zassert_ok(temp_alarm_check(),
               "check must succeed");

    zassert_ok(temp_alarm_get_status(&status),
               "get_status must succeed");

    zassert_equal(status.last_temp, 40,
                  "last_temp must be 40");

    zassert_true(status.is_alarming,
                 "alarm must be active");

    zassert_equal(status.alarm_count, 1U,
                  "alarm_count must be 1");
}

/*
 * ============================================================
 * Suite: temp_alarm_threshold
 * ============================================================
 */

ZTEST_SUITE(temp_alarm_threshold, NULL, NULL, before, NULL, NULL);

/*
 * Test 4: Changing threshold clears the alarm.
 *
 * Initial threshold: 30 C
 * Temperature: 40 C
 * New threshold: 50 C
 *
 * Expected:
 * - First check raises alarm.
 * - Changing threshold clears alarm.
 * - Second check does not raise alarm.
 * - Alarm count remains 1.
 */
ZTEST(temp_alarm_threshold, test_new_threshold_clears_alarm)
{
    struct temp_alarm_status status;

    sensor_fake_set_temperature(40);

    zassert_ok(temp_alarm_check(),
               "first check must succeed");

    zassert_ok(temp_alarm_get_status(&status),
               "get_status must succeed");

    zassert_true(status.is_alarming,
                 "alarm must be active");

    zassert_ok(temp_alarm_set_threshold(50),
               "set_threshold must succeed");

    zassert_ok(temp_alarm_get_status(&status),
               "get_status must succeed");

    zassert_false(status.is_alarming,
                  "changing threshold must clear alarm");

    zassert_ok(temp_alarm_check(),
               "second check must succeed");

    zassert_ok(temp_alarm_get_status(&status),
               "get_status must succeed");

    zassert_false(status.is_alarming,
                  "alarm must remain inactive");

    zassert_equal(status.alarm_count, 1U,
                  "alarm_count must remain 1");
}

/*
 * ============================================================
 * Suite: temp_alarm_reset
 * ============================================================
 */

ZTEST_SUITE(temp_alarm_reset, NULL, NULL, before, NULL, NULL);

/*
 * Test 5: Reset clears alarm state.
 *
 * Temperature: 40 C
 * Threshold: 30 C
 *
 * Perform three checks, then reset.
 *
 * Expected after reset:
 * - Alarm inactive
 * - Alarm count = 0
 * - Last error = 0
 * - Last temperature remains 40 C
 */
ZTEST(temp_alarm_reset, test_reset_clears_state)
{
    struct temp_alarm_status status;

    sensor_fake_set_temperature(40);

    for (int i = 0; i < 3; i++) {
        zassert_ok(temp_alarm_check(),
                   "check must succeed");
    }

    zassert_ok(temp_alarm_get_status(&status),
               "get_status must succeed");

    zassert_equal(status.alarm_count, 3U,
                  "alarm_count must be 3");

    temp_alarm_reset();

    zassert_ok(temp_alarm_get_status(&status),
               "get_status must succeed");

    zassert_false(status.is_alarming,
                  "alarm must be inactive after reset");

    zassert_equal(status.alarm_count, 0U,
                  "alarm_count must be zero after reset");

    zassert_equal(status.last_error, 0,
                  "last_error must be zero after reset");

    zassert_equal(status.last_temp, 40,
                  "last_temp must remain 40");
}

