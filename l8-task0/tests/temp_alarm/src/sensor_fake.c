/*
 * Sensor Fake - definitions, simulated temperature and the fake device
 */


#include "sensor_fake.h"

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/fff.h>
#include <zephyr/toolchain.h>

/*
 * Define the global FFF state.
 *
 * Exactly one DEFINE_FFF_GLOBALS is required
 * per test executable.
 */
DEFINE_FFF_GLOBALS;

/*
 * Fake for sensor_driver_api.sample_fetch.
 */
DEFINE_FAKE_VALUE_FUNC(
    int,
    fake_sensor_sample_fetch,
    const struct device *,
    enum sensor_channel
);

/*
 * Fake for sensor_driver_api.channel_get.
 */
DEFINE_FAKE_VALUE_FUNC(
    int,
    fake_sensor_channel_get,
    const struct device *,
    enum sensor_channel,
    struct sensor_value *
);

/*
 * ------------------------------------------------------------
 * Simulated sensor state
 * ------------------------------------------------------------
 */

static int sim_temperature;

/*
 * Return the simulated temperature.
 *
 * val1 contains whole degrees Celsius.
 * val2 contains the fractional part in millionths.
 */
static int custom_sensor_channel_get(
    const struct device *dev,
    enum sensor_channel chan,
    struct sensor_value *val)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(chan);

    val->val1 = sim_temperature;
    val->val2 = 0;

    return 0;
}

/*
 * ------------------------------------------------------------
 * Fake sensor device
 * ------------------------------------------------------------
 */

/*
 * Register the fake sensor API in Zephyr's sensor API section.
 *
 * This is required by the API validation performed by
 * sensor_sample_fetch() and sensor_channel_get().
 */
static DEVICE_API(sensor, fake_sensor_api) = {
    .sample_fetch = fake_sensor_sample_fetch,
    .channel_get = fake_sensor_channel_get,
};

/*
 * Make device_is_ready() return true.
 */
static struct device_state fake_sensor_state = {
    .init_res = 0,
    .initialized = 1,
};

/*
 * Fake device exposed to the unit tests.
 */
struct device fake_sensor_dev = {
    .name = "fake_sensor",
    .api = &fake_sensor_api,
    .state = &fake_sensor_state,
};

/*
 * ------------------------------------------------------------
 * Public helper functions
 * ------------------------------------------------------------
 */

void sensor_fake_set_temperature(int temp_celsius)
{
    sim_temperature = temp_celsius;
}

/*
 * Install the custom channel_get implementation.
 */
void sensor_fake_init(void)
{
    fake_sensor_channel_get_fake.custom_fake =
        custom_sensor_channel_get;
}

/*
 * Reset the fake sensor before each test.
 */
void sensor_fake_reset(void)
{
    RESET_FAKE(fake_sensor_sample_fetch);
    RESET_FAKE(fake_sensor_channel_get);

    FFF_RESET_HISTORY();

    sim_temperature = 0;
}
