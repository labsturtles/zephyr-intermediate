#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "sensor_iface.h"

LOG_MODULE_REGISTER(l4_task1, LOG_LEVEL_INF);


/*
 * ============================================================================
 * DISPLAY LISTENER
 * ============================================================================
 *
 * The listener executes synchronously in the publisher context.
 *
 * Therefore:
 *   - it must be fast
 *   - it must not block
 *   - it must not call k_sleep()
 *   - it must not perform slow operations
 */

static void display_cb(const struct zbus_channel *chan)
{
	const struct sensor_data *msg;

	msg = zbus_chan_const_msg(chan);

	LOG_INF("DISPLAY: Temperature = %d mC, timestamp = %u ms",
		msg->temperature_mc,
		msg->timestamp_ms);
}


/*
 * Define the display listener.
 */
ZBUS_LISTENER_DEFINE(display_lis, display_cb);


/*
 * ============================================================================
 * LOGGER SUBSCRIBER
 * ============================================================================
 *
 * The subscriber uses an internal notification queue.
 *
 * The value 4 means that up to 4 pending notifications can be stored.
 */

ZBUS_SUBSCRIBER_DEFINE(logger_sub, 4);


/*
 * ============================================================================
 * ZBUS CHANNEL
 * ============================================================================
 *
 * One channel is used for sensor data.
 *
 * Observers:
 *
 *   display_lis -> synchronous listener
 *   logger_sub  -> asynchronous subscriber
 */

ZBUS_CHAN_DEFINE(
	sensor_chan,
	struct sensor_data,
	NULL,
	NULL,
	ZBUS_OBSERVERS(display_lis, logger_sub),
	ZBUS_MSG_INIT(
		.temperature_mc = 0,
		.timestamp_ms = 0
	)
);


/*
 * ============================================================================
 * LOGGER THREAD
 * ============================================================================
 *
 * The subscriber has its own thread.
 *
 * This allows slow processing without blocking the producer.
 */

#define LOGGER_STACK_SIZE	1024
#define LOGGER_PRIORITY		5

K_THREAD_STACK_DEFINE(logger_stack, LOGGER_STACK_SIZE);

static struct k_thread logger_thread_data;


static void logger_thread(void *p1, void *p2, void *p3)
{
	const struct zbus_channel *chan;
	struct sensor_data msg;
	int ret;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_INF("LOGGER thread started");

	while (1) {

		/*
		 * Wait until a notification is available.
		 */
		ret = zbus_sub_wait(
			&logger_sub,
			&chan,
			K_FOREVER
		);

		if (ret != 0) {
			LOG_ERR("zbus_sub_wait() failed: %d", ret);
			continue;
		}

		/*
		 * Read the latest value from the channel.
		 */
		ret = zbus_chan_read(
			chan,
			&msg,
			K_MSEC(100)
		);

		if (ret != 0) {
			LOG_ERR("zbus_chan_read() failed: %d", ret);
			continue;
		}

		/*
		 * Simulate logging.
		 */
		LOG_INF("LOGGER: Temperature = %d mC, timestamp = %u ms",
			msg.temperature_mc,
			msg.timestamp_ms);

		/*
		 * Simulate a slow logger.
		 *
		 * The sensor publishes every 100 ms,
		 * while the logger takes 250 ms to process a sample.
		 */
		k_msleep(250);
	}
}


/*
 * ============================================================================
 * SENSOR PRODUCER
 * ============================================================================
 */

#define SENSOR_STACK_SIZE	1024
#define SENSOR_PRIORITY		4
#define SENSOR_PERIOD_MS	100

K_THREAD_STACK_DEFINE(sensor_stack, SENSOR_STACK_SIZE);

static struct k_thread sensor_thread_data;


static void sensor_thread(void *p1, void *p2, void *p3)
{
	struct sensor_data data;
	uint32_t sample = 0;
	int ret;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_INF("SENSOR thread started");

	while (1) {

		/*
		 * Generate a simulated temperature.
		 *
		 * 24000 mC = 24.0 degrees C
		 *
		 * Each sample increases the temperature by 0.1 degrees C.
		 */
		data.temperature_mc =
			24000 + ((sample % 10) * 100);

		/*
		 * Store the current timestamp.
		 */
		data.timestamp_ms = k_uptime_get_32();

		LOG_INF(
			"SENSOR: publishing sample %u -> %d mC",
			sample,
			data.temperature_mc
		);

		/*
		 * Publish the message through zbus.
		 *
		 * This causes:
		 *
		 *   1. Immediate execution of the listener.
		 *   2. Notification of the subscriber.
		 */
		ret = zbus_chan_pub(
			&sensor_chan,
			&data,
			K_MSEC(100)
		);

		if (ret != 0) {
			LOG_ERR(
				"zbus_chan_pub() failed: %d",
				ret
			);
		}

		sample++;

		/*
		 * Publish a new sample every 100 ms.
		 */
		k_msleep(SENSOR_PERIOD_MS);
	}
}


/*
 * ============================================================================
 * MAIN
 * ============================================================================
 */

int main(void)
{
	LOG_INF("======================================");
	LOG_INF("L4 TASK 1 - ZBUS EVENT-DRIVEN SYSTEM");
	LOG_INF("======================================");

	LOG_INF("Starting sensor producer...");
	LOG_INF("Starting logger subscriber...");


	/*
	 * Create the sensor producer thread.
	 */
	k_thread_create(
		&sensor_thread_data,
		sensor_stack,
		K_THREAD_STACK_SIZEOF(sensor_stack),
		sensor_thread,
		NULL,
		NULL,
		NULL,
		SENSOR_PRIORITY,
		0,
		K_NO_WAIT
	);


	/*
	 * Create the logger subscriber thread.
	 */
	k_thread_create(
		&logger_thread_data,
		logger_stack,
		K_THREAD_STACK_SIZEOF(logger_stack),
		logger_thread,
		NULL,
		NULL,
		NULL,
		LOGGER_PRIORITY,
		0,
		K_NO_WAIT
	);


	return 0;
}
