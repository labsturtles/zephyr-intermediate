/*
 * Sensor Fake - FFF fakes behind a fake struct device
 */

#ifndef SENSOR_FAKE_H
#define SENSOR_FAKE_H

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/fff.h>

/*
 * Fake for sensor_driver_api.sample_fetch.
 *
 * Called when the temperature alarm requests
 * a new sensor sample.
 */
DECLARE_FAKE_VALUE_FUNC(
    int,
    fake_sensor_sample_fetch,
    const struct device *,
    enum sensor_channel
);

/*
 * Fake for sensor_driver_api.channel_get.
 *
 * Called when the temperature alarm reads
 * the temperature value.
 */
DECLARE_FAKE_VALUE_FUNC(
    int,
    fake_sensor_channel_get,
    const struct device *,
    enum sensor_channel,
    struct sensor_value *
);

/*
 * Fake sensor device passed to temp_alarm_init().
 */
extern struct device fake_sensor_dev;

/*
 * Set the simulated temperature in degrees Celsius.
 */
void sensor_fake_set_temperature(int temp_celsius);

/*
 * Install the custom implementation for channel_get.
 *
 * Call after sensor_fake_reset(), because RESET_FAKE()
 * clears the custom_fake callback.
 */
void sensor_fake_init(void);

/*
 * Reset all FFF fakes, call history and temperature.
 */
void sensor_fake_reset(void);

#endif /* SENSOR_FAKE_H */
