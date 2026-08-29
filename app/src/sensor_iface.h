#ifndef SENSOR_IFACE_H
#define SENSOR_IFACE_H

#include <stdint.h>
#include <zephyr/zbus/zbus.h>


/*
 * Sensor data structure.
 */
struct sensor_data {
	int32_t temperature_mc;
	uint32_t timestamp_ms;
};


/*
 * Declaration of the zbus channel.
 *
 * The channel is defined in main.c using ZBUS_CHAN_DEFINE().
 */
ZBUS_CHAN_DECLARE(sensor_chan);


#endif /* SENSOR_IFACE_H */
