#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/tracing/tracing.h>

LOG_MODULE_REGISTER(homework, LOG_LEVEL_INF);


/*
 * ============================================================================
 * L6 HOMEWORK - SCHEDULING DELAY INVESTIGATION
 * ============================================================================
 *
 * Response-time guarantee:
 *
 *   The control thread must start processing each event within 10 ms
 *   from the time the timer creates the event.
 *
 * The application measures this latency for every sequence and counts
 * every deadline miss.
 */


/*
 * ============================================================================
 * CONFIGURATION
 * ============================================================================
 */

#define STACK_SIZE                 2048

#define EVENT_PERIOD_MS             250
#define MAINTENANCE_LOAD_US       45000

#define CONTROL_DEADLINE_MS           10


/*
 * Set this value to:
 *
 *   0 -> Original configuration (BEFORE correction)
 *   1 -> Corrected configuration (AFTER correction)
 *
 * Only one cause is changed: thread priority.
 */
#define APPLY_SCHEDULING_FIX            1


/*
 * Original configuration:
 *
 *   maintenance priority = 4
 *   control priority     = 7
 *
 * In Zephyr, a lower numerical value means a higher scheduling priority.
 *
 * Therefore, the maintenance thread can run before the time-critical
 * control thread.
 *
 * Corrected configuration:
 *
 *   control priority     = 4
 *   maintenance priority = 7
 */
#if APPLY_SCHEDULING_FIX

#define CONTROL_PRIORITY              4
#define MAINTENANCE_PRIORITY          7

#else

#define CONTROL_PRIORITY              7
#define MAINTENANCE_PRIORITY          4

#endif


/*
 * ============================================================================
 * CONTROL EVENT
 * ============================================================================
 *
 * Each event contains:
 *
 *   seq      -> stable sequence identifier
 *   ready_ms -> timestamp when the event became available
 */

struct control_event {
	uint32_t seq;
	uint32_t ready_ms;
};


/*
 * Queue used by the producer/consumer pipeline.
 */
K_MSGQ_DEFINE(control_queue,
	      sizeof(struct control_event),
	      4,
	      4);


/*
 * Semaphore used to activate the maintenance thread.
 */
K_SEM_DEFINE(maintenance_start, 0, 1);


/*
 * Deadline statistics.
 */
static uint32_t deadline_misses;
static uint32_t events_processed;
static uint32_t max_latency_ms;


/*
 * ============================================================================
 * TIMER EXPIRY - EVENT PRODUCER
 * ============================================================================
 *
 * The timer callback creates one control event.
 *
 * The callback executes in interrupt context, therefore it must never block.
 */

static void event_timer_expiry(struct k_timer *timer)
{
	static uint32_t seq;

	struct control_event event = {
		.seq = seq++,
		.ready_ms = k_uptime_get_32(),
	};

	ARG_UNUSED(timer);


	/*
	 * Insert the event without waiting because this function executes
	 * in interrupt context.
	 */
	int ret = k_msgq_put(&control_queue, &event, K_NO_WAIT);

	if (ret != 0) {
		return;
	}


	/*
	 * Generate an application trace event.
	 *
	 * This provides a stable sequence identifier that can later be
	 * correlated with scheduler events in the CTF trace.
	 */
	sys_trace_named_event(
		"event_ready",
		event.seq,
		k_msgq_num_used_get(&control_queue)
	);


	/*
	 * Both the control and maintenance threads become ready when the
	 * timer interrupt returns.
	 */
	k_sem_give(&maintenance_start);
}


K_TIMER_DEFINE(event_timer, event_timer_expiry, NULL);


/*
 * ============================================================================
 * CONTROL THREAD
 * ============================================================================
 *
 * This is the time-critical consumer.
 *
 * Response-time guarantee:
 *
 *     event ready -> control processing start <= 10 ms
 */

static void control_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {

		struct control_event event;

		int ret = k_msgq_get(
			&control_queue,
			&event,
			K_FOREVER
		);

		if (ret != 0) {
			LOG_ERR("[CONTROL] receive failed: %d", ret);
			continue;
		}


		/*
		 * Measure the response time from event creation until the
		 * control thread starts processing it.
		 */
		uint32_t now_ms = k_uptime_get_32();
		uint32_t latency_ms = now_ms - event.ready_ms;

		events_processed++;


		/*
		 * Track the maximum observed latency.
		 */
		if (latency_ms > max_latency_ms) {
			max_latency_ms = latency_ms;
		}


		/*
		 * Sequence-based structured log.
		 */
		LOG_INF(
			"event_done seq=%u latency_ms=%u misses=%u max_ms=%u",
			event.seq,
			latency_ms,
			deadline_misses,
			max_latency_ms
		);


		/*
		 * Count every deadline violation.
		 *
		 * The counter is always updated, while the warning output is
		 * rate-limited to reduce diagnostic overhead.
		 */
		if (latency_ms > CONTROL_DEADLINE_MS) {

			deadline_misses++;

			LOG_WRN_RATELIMIT(
				"deadline_miss count=%u seq=%u latency_ms=%u",
				deadline_misses,
				event.seq,
				latency_ms
			);
		}


		/*
		 * Generate a completion trace event.
		 */
		sys_trace_named_event(
			"event_done",
			event.seq,
			latency_ms
		);
	}
}


/*
 * ============================================================================
 * BACKGROUND MAINTENANCE THREAD
 * ============================================================================
 *
 * This work is CPU intensive but does not have a short response-time
 * requirement.
 *
 * The original application intentionally gives this thread a higher
 * priority than the control thread.
 */

static void maintenance_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {

		k_sem_take(
			&maintenance_start,
			K_FOREVER
		);


		/*
		 * Simulate CPU-intensive maintenance work.
		 */
		k_busy_wait(MAINTENANCE_LOAD_US);
	}
}


/*
 * ============================================================================
 * THREAD DEFINITIONS
 * ============================================================================
 */

K_THREAD_DEFINE(
	control,
	STACK_SIZE,
	control_fn,
	NULL,
	NULL,
	NULL,
	CONTROL_PRIORITY,
	0,
	0
);


K_THREAD_DEFINE(
	maintenance,
	STACK_SIZE,
	maintenance_fn,
	NULL,
	NULL,
	NULL,
	MAINTENANCE_PRIORITY,
	0,
	0
);


/*
 * ============================================================================
 * MAIN
 * ============================================================================
 */

int main(void)
{
	LOG_INF("================================================");
	LOG_INF("L6 TASK 1 - SCHEDULING DELAY INVESTIGATION");
	LOG_INF("================================================");

	LOG_INF(
		"Response-time guarantee: control <= %d ms",
		CONTROL_DEADLINE_MS
	);

	LOG_INF(
		"Control priority     : %d",
		CONTROL_PRIORITY
	);

	LOG_INF(
		"Maintenance priority : %d",
		MAINTENANCE_PRIORITY
	);

	LOG_INF(
		"Maintenance load     : %d us",
		MAINTENANCE_LOAD_US
	);


#if APPLY_SCHEDULING_FIX

	LOG_INF("Test configuration   : AFTER correction");

#else

	LOG_INF("Test configuration   : BEFORE correction");

#endif


	LOG_INF("Starting event timer...");


	/*
	 * Start after 500 ms and generate one event every 250 ms.
	 */
	k_timer_start(
		&event_timer,
		K_MSEC(500),
		K_MSEC(EVENT_PERIOD_MS)
	);


	return 0;
}
