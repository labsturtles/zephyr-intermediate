
/*
 * temp_alarm - Error-path unit tests
 *
 * Module 8 - Task 2
 *
 * Tests:
 * - Sensor sample fetch failure
 * - Sensor channel read failure
 * - Recovery after a sensor failure
 * - NULL status pointer
 * - NULL sensor device
 * - Invalid initialization thresholds
 * - Invalid runtime thresholds
 */

#include <errno.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include "sensor_fake.h"
#include "temp_alarm.h"

#define DEFAULT_THRESHOLD 30

/*
 * Reset the sensor fakes and initialize the module
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
 * Suite: temp_alarm_sensor_errors
 * ============================================================
 */

ZTEST_SUITE(temp_alarm_sensor_errors, NULL, NULL, before, NULL, NULL);

/*
 * Test 1: Sample fetch failure.
 *
 * Expected:
 * - temp_alarm_check() returns -EIO.
 * - last_error becomes -EIO.
 * - channel_get is not called.
 */
ZTEST(temp_alarm_sensor_errors, test_fetch_error_propagates)
{
    struct temp_alarm_status status;

    fake_sensor_sample_fetch_fake.return_val = -EIO;

    zassert_equal(temp_alarm_check(), -EIO,
                  "check must return -EIO");

    zassert_ok(temp_alarm_get_status(&status),
               "get_status must succeed");

    zassert_equal(status.last_error, -EIO,
                  "last_error must be -EIO");

    zassert_equal(fake_sensor_sample_fetch_fake.call_count, 1,
                  "sample_fetch must be called once");

    zassert_equal(fake_sensor_channel_get_fake.call_count, 0,
                  "channel_get must not be called");
}

/*
 * Test 2: Channel read failure.
 *
 * Expected:
 * - sample_fetch succeeds.
 * - channel_get returns -EIO.
 * - temp_alarm_check() returns -EIO.
 * - last_error becomes -EIO.
 */
ZTEST(temp_alarm_sensor_errors, test_channel_get_error_propagates)
{
    struct temp_alarm_status status;

    /*
     * The before hook installed a custom_fake for channel_get.
     * Disable it so FFF uses return_val instead.
     */
    fake_sensor_channel_get_fake.custom_fake = NULL;
    fake_sensor_channel_get_fake.return_val = -EIO;

    zassert_equal(temp_alarm_check(), -EIO,
                  "check must return -EIO");

    zassert_ok(temp_alarm_get_status(&status),
               "get_status must succeed");

    zassert_equal(status.last_error, -EIO,
                  "last_error must be -EIO");

    zassert_equal(fake_sensor_sample_fetch_fake.call_count, 1,
                  "sample_fetch must be called once");

    zassert_equal(fake_sensor_channel_get_fake.call_count, 1,
                  "channel_get must be called once");
}

/*
 * Test 3: Failure followed by recovery.
 *
 * First check:
 * - sample_fetch returns -EIO.
 *
 * Second check:
 * - sample_fetch succeeds.
 * - temperature is 20 C.
 *
 * Expected:
 * - Module recovers.
 * - Alarm remains inactive.
 * - last_error keeps the previous error.
 */
ZTEST(temp_alarm_sensor_errors, test_fail_then_recover)
{
    struct temp_alarm_status status;

    int fetch_results[] = {
        -EIO,
        0
    };

    SET_RETURN_SEQ(
        fake_sensor_sample_fetch,
        fetch_results,
        2
    );

    sensor_fake_set_temperature(20);

    zassert_equal(temp_alarm_check(), -EIO,
                  "first check must return -EIO");

    zassert_ok(temp_alarm_check(),
               "second check must succeed");

    zassert_ok(temp_alarm_get_status(&status),
               "get_status must succeed");

    zassert_equal(status.last_temp, 20,
                  "last_temp must be 20");

    zassert_false(status.is_alarming,
                  "alarm must remain inactive");

    zassert_equal(status.last_error, -EIO,
                  "last_error must retain -EIO");

    zassert_equal(fake_sensor_sample_fetch_fake.call_count, 2,
                  "sample_fetch must be called twice");

    zassert_equal(fake_sensor_channel_get_fake.call_count, 1,
                  "channel_get must be called once");
}

/*
 * ============================================================
 * Suite: temp_alarm_invalid_input
 * ============================================================
 */

ZTEST_SUITE(temp_alarm_invalid_input, NULL, NULL, before, NULL, NULL);

/*
 * Test 4: NULL status pointer.
 *
 * Expected: -EINVAL.
 */
ZTEST(temp_alarm_invalid_input, test_get_status_null)
{
    zassert_equal(temp_alarm_get_status(NULL), -EINVAL,
                  "NULL status must return -EINVAL");
}

/*
 * Test 5: NULL sensor device.
 *
 * Expected: -ENODEV.
 */
ZTEST(temp_alarm_invalid_input, test_init_without_device)
{
    zassert_equal(
        temp_alarm_init(NULL, DEFAULT_THRESHOLD),
        -ENODEV,
        "NULL device must return -ENODEV"
    );
}

/*
 * Test 6: Initialization threshold out of range.
 *
 * Valid range: 0 to 100 C.
 *
 * Expected:
 * - Threshold -1 is rejected.
 * - Threshold 101 is rejected.
 */
ZTEST(temp_alarm_invalid_input, test_init_threshold_out_of_range)
{
    zassert_equal(
        temp_alarm_init(&fake_sensor_dev, -1),
        -EINVAL,
        "negative threshold must return -EINVAL"
    );

    zassert_equal(
        temp_alarm_init(&fake_sensor_dev, 101),
        -EINVAL,
        "threshold above 100 must return -EINVAL"
    );
}

/*
 * Test 7: Runtime threshold out of range.
 *
 * Expected:
 * - -1 and 101 are rejected.
 * - Original threshold (30 C) remains active.
 * - A reading of 30 C does not raise the alarm.
 * - A reading of 40 C raises the alarm.
 */
ZTEST(temp_alarm_invalid_input, test_set_threshold_out_of_range)
{
    struct temp_alarm_status status;

    zassert_equal(
        temp_alarm_set_threshold(-1),
        -EINVAL,
        "negative threshold must return -EINVAL"
    );

    zassert_equal(
        temp_alarm_set_threshold(101),
        -EINVAL,
        "threshold above 100 must return -EINVAL"
    );

    /*
     * Temperature at the original threshold.
     * This must not trigger an alarm.
     */
    sensor_fake_set_temperature(30);

    zassert_ok(temp_alarm_check(),
               "check at 30 C must succeed");

    zassert_ok(temp_alarm_get_status(&status),
               "get_status must succeed");

    zassert_false(status.is_alarming,
                  "30 C must not trigger an alarm");

    zassert_equal(status.alarm_count, 0U,
                  "alarm_count must remain zero");

    /*
     * Temperature above the original threshold.
     * This verifies that 30 C is still in force.
     */
    sensor_fake_set_temperature(40);

    zassert_ok(temp_alarm_check(),
               "check at 40 C must succeed");

    zassert_ok(temp_alarm_get_status(&status),
               "get_status must succeed");

    zassert_true(status.is_alarming,
                 "40 C must trigger an alarm");

    zassert_equal(status.alarm_count, 1U,
                  "alarm_count must be 1");
}

