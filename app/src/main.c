#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/task_wdt/task_wdt.h>

LOG_MODULE_REGISTER(l5_task1, LOG_LEVEL_INF);


/*
 * ============================================================================
 * CONFIGURATION
 * ============================================================================
 */

/* Message queue configuration */
#define QUEUE_DEPTH             8

/* Producer creates one sample every 200 ms */
#define PRODUCER_PERIOD_MS      200

/* Normal consumer processing time */
#define CONSUMER_PROCESS_MS     100

/*
 * Simulated consumer stall.
 *
 * The watchdog timeout is shorter than this value, so the watchdog callback
 * will be triggered while the consumer is sleeping.
 */
#define CONSUMER_STUCK_MS       5000

/* Consumer will simulate a failure after processing this many samples */
#define STUCK_AFTER_SAMPLES     10

/* Task watchdog timeout */
#define CONSUMER_WDT_TIMEOUT_MS 2000

/* Health-check period */
#define HEALTH_CHECK_PERIOD_MS  250


/*
 * Thread configuration.
 */
#define PRODUCER_STACK_SIZE     1024
#define CONSUMER_STACK_SIZE     1024
#define HEALTH_STACK_SIZE       1024

#define PRODUCER_PRIORITY       5
#define CONSUMER_PRIORITY       5
#define HEALTH_PRIORITY         6


/*
 * ============================================================================
 * SENSOR MESSAGE
 * ============================================================================
 */

struct sensor_data {
	uint32_t sequence;
	int32_t temperature_mc;
	uint32_t timestamp_ms;
};


/*
 * ============================================================================
 * MESSAGE QUEUE
 * ============================================================================
 *
 * The queue contains copies of struct sensor_data.
 *
 * Depth = 8 messages.
 */

K_MSGQ_DEFINE(sensor_queue,
	      sizeof(struct sensor_data),
	      QUEUE_DEPTH,
	      4);


/*
 * ============================================================================
 * THREAD OBJECTS
 * ============================================================================
 */

K_THREAD_STACK_DEFINE(producer_stack, PRODUCER_STACK_SIZE);
K_THREAD_STACK_DEFINE(consumer_stack, CONSUMER_STACK_SIZE);
K_THREAD_STACK_DEFINE(health_stack, HEALTH_STACK_SIZE);

static struct k_thread producer_thread_data;
static struct k_thread consumer_thread_data;
static struct k_thread health_thread_data;


/*
 * Task watchdog channel used to supervise the consumer thread.
 */
static int consumer_wdt_channel = -1;


/*
 * ============================================================================
 * TASK WATCHDOG CALLBACK
 * ============================================================================
 *
 * This callback is executed when the consumer does not feed its task watchdog
 * before the configured timeout expires.
 */

static void consumer_wdt_callback(int channel_id, void *user_data)
{
	ARG_UNUSED(user_data);

	LOG_ERR("================================================");
	LOG_ERR("TASK WATCHDOG FIRED!");
	LOG_ERR("Consumer thread is not responding");
	LOG_ERR("Watchdog channel: %d", channel_id);
	LOG_ERR("================================================");
}


/*
 * ============================================================================
 * PRODUCER THREAD
 * ============================================================================
 *
 * Generates sensor samples periodically and tries to insert them into the
 * message queue.
 *
 * K_NO_WAIT is used intentionally so that the producer does not block when
 * the queue becomes full.
 */

static void producer_thread(void *p1, void *p2, void *p3)
{
	struct sensor_data data;
	uint32_t sequence = 0;
	int ret;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_INF("PRODUCER thread started");

	while (1) {

		data.sequence = sequence;

		/*
		 * Generate a simulated temperature.
		 *
		 * 24000 mC = 24.0 degrees C.
		 */
		data.temperature_mc =
			24000 + ((sequence % 10) * 100);

		data.timestamp_ms = k_uptime_get_32();

		/*
		 * Try to add the sample without blocking.
		 */
		ret = k_msgq_put(&sensor_queue, &data, K_NO_WAIT);

		if (ret == 0) {
			LOG_INF("PRODUCER: sample %u -> %d mC",
				data.sequence,
				data.temperature_mc);
		} else {
			/*
			 * The queue is full.
			 *
			 * Drop the newest sample instead of blocking the producer.
			 */
			LOG_WRN("PRODUCER: queue FULL - dropping sample %u",
				data.sequence);
		}

		sequence++;

		k_msleep(PRODUCER_PERIOD_MS);
	}
}


/*
 * ============================================================================
 * CONSUMER THREAD
 * ============================================================================
 *
 * The consumer normally receives and processes messages from the queue.
 *
 * After STUCK_AFTER_SAMPLES messages it intentionally performs a long sleep,
 * simulating a stuck or unresponsive consumer.
 *
 * While the consumer is sleeping:
 *
 *   - the producer continues producing data
 *   - the queue begins to fill
 *   - the health-check thread detects high queue usage
 *   - the task watchdog expires
 */

static void consumer_thread(void *p1, void *p2, void *p3)
{
	struct sensor_data data;
	uint32_t processed_samples = 0;
	bool stuck_simulated = false;
	int ret;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_INF("CONSUMER thread started");

	while (1) {

		/*
		 * Wait indefinitely for the next message.
		 */
		ret = k_msgq_get(&sensor_queue, &data, K_FOREVER);

		if (ret != 0) {
			LOG_ERR("CONSUMER: k_msgq_get() failed: %d", ret);
			continue;
		}

		LOG_INF("CONSUMER: sample %u -> %d mC",
			data.sequence,
			data.temperature_mc);

		processed_samples++;

		/*
		 * Feed the task watchdog because the consumer is alive and
		 * successfully processing data.
		 */
		if (consumer_wdt_channel >= 0) {
			ret = task_wdt_feed(consumer_wdt_channel);

			if (ret != 0) {
				LOG_ERR("CONSUMER: task_wdt_feed() failed: %d",
					ret);
			}
		}

		/*
		 * Simulate the consumer becoming stuck.
		 *
		 * This is done only once so that the application can recover
		 * after the demonstration.
		 */
		if (!stuck_simulated &&
		    processed_samples >= STUCK_AFTER_SAMPLES) {

			stuck_simulated = true;

			LOG_WRN("================================================");
			LOG_WRN("CONSUMER: SIMULATING STUCK THREAD");
			LOG_WRN("CONSUMER: sleeping for %d ms",
				CONSUMER_STUCK_MS);
			LOG_WRN("================================================");

			/*
			 * No watchdog feed occurs during this sleep.
			 */
			k_msleep(CONSUMER_STUCK_MS);

			LOG_INF("CONSUMER: recovered from simulated stall");

			/*
			 * Feed the watchdog immediately after recovery.
			 */
			if (consumer_wdt_channel >= 0) {
				task_wdt_feed(consumer_wdt_channel);
			}
		}

		/*
		 * Simulate normal processing time.
		 */
		k_msleep(CONSUMER_PROCESS_MS);
	}
}


/*
 * ============================================================================
 * HEALTH-CHECK THREAD
 * ============================================================================
 *
 * Periodically checks the queue fill level.
 *
 * The homework requires a warning when the queue reaches 75% capacity.
 *
 * Queue depth = 8
 *
 * 75% of 8 = 6 messages
 */

static void health_thread(void *p1, void *p2, void *p3)
{
	uint32_t used;
	uint32_t percentage;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_INF("HEALTH-CHECK thread started");

	while (1) {

		used = k_msgq_num_used_get(&sensor_queue);

		percentage = (used * 100U) / QUEUE_DEPTH;

		/*
		 * Log a warning when queue usage reaches 75%.
		 */
		if (used >= ((QUEUE_DEPTH * 3U) / 4U)) {

			LOG_WRN("HEALTH: queue at %u/%u (%u%%)",
				used,
				QUEUE_DEPTH,
				percentage);
		}

		k_msleep(HEALTH_CHECK_PERIOD_MS);
	}
}


/*
 * ============================================================================
 * MAIN
 * ============================================================================
 */

int main(void)
{
	int ret;

	LOG_INF("==============================================");
	LOG_INF("L5 TASK 1 - RELIABILITY UNDER PRESSURE");
	LOG_INF("==============================================");

	LOG_INF("Queue depth       : %d", QUEUE_DEPTH);
	LOG_INF("Producer period   : %d ms", PRODUCER_PERIOD_MS);
	LOG_INF("Consumer WDT      : %d ms", CONSUMER_WDT_TIMEOUT_MS);
	LOG_INF("Consumer stall    : %d ms", CONSUMER_STUCK_MS);

	/*
	 * Initialize the task watchdog.
	 *
	 * NULL means that no hardware watchdog is used as fallback.
	 * This exercise uses the software task watchdog only.
	 */
	ret = task_wdt_init(NULL);

	if (ret != 0) {
		LOG_ERR("task_wdt_init() failed: %d", ret);
		return 0;
	}

	LOG_INF("Task watchdog initialized");

	/*
	 * Register a watchdog channel for the consumer.
	 *
	 * The consumer must feed this watchdog periodically.
	 */
	consumer_wdt_channel =
		task_wdt_add(CONSUMER_WDT_TIMEOUT_MS,
			     consumer_wdt_callback,
			     NULL);

	if (consumer_wdt_channel < 0) {
		LOG_ERR("task_wdt_add() failed: %d",
			consumer_wdt_channel);
		return 0;
	}

	LOG_INF("Consumer watchdog channel: %d",
		consumer_wdt_channel);


	/*
	 * Create the producer thread.
	 */
	k_thread_create(
		&producer_thread_data,
		producer_stack,
		K_THREAD_STACK_SIZEOF(producer_stack),
		producer_thread,
		NULL,
		NULL,
		NULL,
		PRODUCER_PRIORITY,
		0,
		K_NO_WAIT
	);


	/*
	 * Create the consumer thread.
	 */
	k_thread_create(
		&consumer_thread_data,
		consumer_stack,
		K_THREAD_STACK_SIZEOF(consumer_stack),
		consumer_thread,
		NULL,
		NULL,
		NULL,
		CONSUMER_PRIORITY,
		0,
		K_NO_WAIT
	);


	/*
	 * Create the health-check thread.
	 */
	k_thread_create(
		&health_thread_data,
		health_stack,
		K_THREAD_STACK_SIZEOF(health_stack),
		health_thread,
		NULL,
		NULL,
		NULL,
		HEALTH_PRIORITY,
		0,
		K_NO_WAIT
	);


	LOG_INF("All threads started");

	return 0;
}
