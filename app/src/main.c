/*
 * Lecture 3 - Homework
 *
 * Deferred Execution & Execution Flow
 *
 * HOMEWORK_TASK:
 *
 *   1 = TASK 1
 *       Starter polling implementation
 *
 *   2 = TASK 2 + TASK 3
 *       Event-driven implementation using k_work
 *
 *   3 = BONUS
 *       Debounce using k_work_reschedule()
 *
 * Only the selected implementation is compiled.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdbool.h>

LOG_MODULE_REGISTER(L3_homework, LOG_LEVEL_DBG);


/* ================================================================
 * SELECT HOMEWORK
 * ================================================================ */

/*
 * Change this value:
 *
 *   1 -> Task 1: Polling
 *   2 -> Task 2 + Task 3: Workqueue
 *   3 -> Bonus: Debounce
 */
#define HOMEWORK_TASK 2


/* ================================================================
 * COMMON CONFIGURATION
 * ================================================================ */

#define STACK_SIZE      1024

#define SENSOR_MS       100
#define POLL_MS         10
#define EVENT_COUNT     10


/* ================================================================
 * BONUS CONFIGURATION
 * ================================================================ */

#define BURST_EVENTS    5
#define BURST_INTERVAL  5
#define DEBOUNCE_MS     30


/* ================================================================
 * TASK 1
 *
 * Starter polling implementation
 *
 * Sensor:
 *     Generates one event every 100 ms.
 *
 * Polling thread:
 *     Wakes every 10 ms and checks sensor_flag.
 *
 * Expected:
 *     Approximately 10 wake-ups per sensor event.
 * ================================================================ */

#if HOMEWORK_TASK == 1


static volatile bool sensor_flag;

static int total_events;
static int total_wakeups;
static int total_processed;


/* ------------------------------------------------
 * Sensor simulator
 * ------------------------------------------------ */

static void sensor_sim_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    for (int i = 0; i < EVENT_COUNT; i++) {

        k_msleep(SENSOR_MS);

        total_events++;

        LOG_INF("[SENSOR] event %d  tick=%u",
                i + 1,
                k_uptime_get_32());

        /*
         * Starter implementation:
         * signal the polling thread using a shared flag.
         */
        sensor_flag = true;
    }

    LOG_INF("[SENSOR] all events produced");
}


/* ------------------------------------------------
 * Polling thread
 * ------------------------------------------------ */

static void polling_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (total_processed < EVENT_COUNT) {

        /*
         * Poll every 10 ms.
         */
        k_msleep(POLL_MS);

        total_wakeups++;

        if (sensor_flag) {

            sensor_flag = false;

            total_processed++;

            LOG_INF("[CONSUMER] processed event %d  "
                    "wakeups_so_far=%d  tick=%u",
                    total_processed,
                    total_wakeups,
                    k_uptime_get_32());
        }
    }


    /* ------------------------------------------------
     * Task 1 summary
     * ------------------------------------------------ */

    LOG_INF("");
    LOG_INF("[SUMMARY] events=%d  total_wakeups=%d  wasted=%d",
            total_processed,
            total_wakeups,
            total_wakeups - total_processed);

    if (total_wakeups > 0) {

        LOG_INF("[SUMMARY] wasted wakeups = %d%% of all wakeups",
                (total_wakeups - total_processed) * 100 /
                total_wakeups);
    }
}


/* ------------------------------------------------
 * Threads
 * ------------------------------------------------ */

K_THREAD_DEFINE(sensor_thread,
                STACK_SIZE,
                sensor_sim_fn,
                NULL,
                NULL,
                NULL,
                5,
                0,
                0);


K_THREAD_DEFINE(polling_thread,
                STACK_SIZE,
                polling_fn,
                NULL,
                NULL,
                NULL,
                5,
                0,
                0);


/* ================================================================
 * TASK 2 + TASK 3
 *
 * Event-driven workqueue implementation
 *
 * The polling thread is removed.
 *
 * sensor_sim_fn()
 *       |
 *       | k_work_submit()
 *       v
 * sensor_work
 *       |
 *       v
 * sensor_handler()
 *
 * Task 3:
 *     k_uptime_get_32() is logged from the handler.
 * ================================================================ */

#elif HOMEWORK_TASK == 2


static int total_events;
static int total_processed;


/* ------------------------------------------------
 * Work handler
 * ------------------------------------------------ */

static void sensor_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    total_processed++;

    LOG_INF("[HANDLER] processed event %d  tick=%u",
            total_processed,
            k_uptime_get_32());


    /*
     * All events have now been processed.
     */
    if (total_processed == EVENT_COUNT) {

        LOG_INF("");
        LOG_INF("[SUMMARY] events=%d  total_processed=%d",
                total_events,
                total_processed);

        LOG_INF("[SUMMARY] polling thread removed");
        LOG_INF("[SUMMARY] handler executes only after "
                "real sensor events");
    }
}


/* ------------------------------------------------
 * Work item
 * ------------------------------------------------ */

K_WORK_DEFINE(sensor_work, sensor_handler);


/* ------------------------------------------------
 * Sensor simulator
 * ------------------------------------------------ */

static void sensor_sim_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    for (int i = 0; i < EVENT_COUNT; i++) {

        k_msleep(SENSOR_MS);

        total_events++;

        LOG_INF("[SENSOR] event %d  tick=%u",
                i + 1,
                k_uptime_get_32());


        /*
         * TASK 2:
         *
         * Instead of setting a flag, submit the work item.
         */
        int ret = k_work_submit(&sensor_work);

        if (ret < 0) {

            LOG_ERR("[SENSOR] k_work_submit() failed: %d",
                    ret);
        }
    }

    LOG_INF("[SENSOR] all events produced");
}


/* ------------------------------------------------
 * Sensor thread
 * ------------------------------------------------ */

K_THREAD_DEFINE(sensor_thread,
                STACK_SIZE,
                sensor_sim_fn,
                NULL,
                NULL,
                NULL,
                5,
                0,
                0);


/* ================================================================
 * BONUS
 *
 * Debounce using k_work_reschedule()
 *
 * Five events:
 *
 *     event 1 -> 0 ms
 *     event 2 -> 5 ms
 *     event 3 -> 10 ms
 *     event 4 -> 15 ms
 *     event 5 -> 20 ms
 *
 * Every event calls:
 *
 *     k_work_reschedule(..., 30 ms)
 *
 * Therefore the 30 ms timer is restarted after every event.
 *
 * Result:
 *
 *     5 sensor events
 *              |
 *              v
 *       one handler call
 *
 * ================================================================ */

#elif HOMEWORK_TASK == 3


static int total_events;
static int total_processed;


/* ------------------------------------------------
 * Debounce handler
 * ------------------------------------------------ */

static void sensor_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    total_processed++;

    LOG_INF("[HANDLER] processed burst  tick=%u  "
            "handler_calls=%d",
            k_uptime_get_32(),
            total_processed);


    LOG_INF("[SUMMARY] events=%d  handler_calls=%d",
            total_events,
            total_processed);
}


/* ------------------------------------------------
 * Delayable work
 * ------------------------------------------------ */

K_WORK_DELAYABLE_DEFINE(debounce_work, sensor_handler);


/* ------------------------------------------------
 * Sensor burst simulator
 * ------------------------------------------------ */

static void sensor_burst_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    LOG_INF("[SENSOR] starting burst of %d events",
            BURST_EVENTS);


    for (int i = 0; i < BURST_EVENTS; i++) {

        total_events++;

        uint32_t timestamp = k_uptime_get_32();

        LOG_INF("[SENSOR] burst event %d  tick=%u",
                i + 1,
                timestamp);


        /*
         * BONUS:
         *
         * Reschedule the work for 30 ms.
         *
         * Every new event resets the 30 ms delay.
         */
        int ret = k_work_reschedule(&debounce_work,
                                    K_MSEC(DEBOUNCE_MS));


        LOG_INF("[DEBOUNCE] reschedule event %d  "
                "tick=%u  delay=%dms",
                i + 1,
                timestamp,
                DEBOUNCE_MS);


        if (ret < 0) {

            LOG_ERR("[DEBOUNCE] reschedule failed: %d",
                    ret);
        }


        /*
         * Generate the burst:
         *
         * 0 ms
         * 5 ms
         * 10 ms
         * 15 ms
         * 20 ms
         */
        if (i < BURST_EVENTS - 1) {

            k_msleep(BURST_INTERVAL);
        }
    }


    LOG_INF("[SENSOR] burst finished  tick=%u",
            k_uptime_get_32());
}


/* ------------------------------------------------
 * Sensor thread
 * ------------------------------------------------ */

K_THREAD_DEFINE(sensor_thread,
                STACK_SIZE,
                sensor_burst_fn,
                NULL,
                NULL,
                NULL,
                5,
                0,
                0);


/* ================================================================
 * INVALID CONFIGURATION
 * ================================================================ */

#else

#error "Invalid HOMEWORK_TASK. Use 1, 2 or 3."

#endif


/* ================================================================
 * MAIN
 * ================================================================ */

int main(void)
{
#if HOMEWORK_TASK == 1

    /* ------------------------------------------------
     * TASK 1
     * ------------------------------------------------ */

    LOG_INF("========================================");
    LOG_INF("L3 HOMEWORK - TASK 1");
    LOG_INF("Polling implementation");
    LOG_INF("========================================");

    LOG_INF("Sensor period : %d ms",
            SENSOR_MS);

    LOG_INF("Polling period: %d ms",
            POLL_MS);

    LOG_INF("Expected wake-ups per event: ~%d",
            SENSOR_MS / POLL_MS);

    LOG_INF("Expected wasted wake-ups per event: ~%d",
            (SENSOR_MS / POLL_MS) - 1);

    LOG_INF("========================================");


    /*
     * Wait until the sensor and polling thread
     * have enough time to finish.
     */
    k_msleep((EVENT_COUNT + 2) * SENSOR_MS + 500);


#elif HOMEWORK_TASK == 2

    /* ------------------------------------------------
     * TASK 2 + TASK 3
     * ------------------------------------------------ */

    LOG_INF("========================================");
    LOG_INF("L3 HOMEWORK - TASK 2 + TASK 3");
    LOG_INF("Event-driven workqueue");
    LOG_INF("========================================");

    LOG_INF("Sensor period: %d ms",
            SENSOR_MS);

    LOG_INF("Events: %d",
            EVENT_COUNT);

    LOG_INF("Polling thread: DISABLED");

    LOG_INF("Workqueue: ENABLED");

    LOG_INF("========================================");


    /*
     * Wait for all sensor events and their handlers.
     */
    k_msleep((EVENT_COUNT + 2) * SENSOR_MS + 500);


#elif HOMEWORK_TASK == 3

    /* ------------------------------------------------
     * BONUS
     * ------------------------------------------------ */

    LOG_INF("========================================");
    LOG_INF("L3 HOMEWORK - BONUS");
    LOG_INF("Debounce with k_work_reschedule()");
    LOG_INF("========================================");

    LOG_INF("Burst events : %d",
            BURST_EVENTS);

    LOG_INF("Burst interval: %d ms",
            BURST_INTERVAL);

    LOG_INF("Debounce delay: %d ms",
            DEBOUNCE_MS);

    LOG_INF("Expected handler calls: 1");

    LOG_INF("========================================");


    /*
     * Wait for:
     *
     * - complete burst (~20 ms)
     * - final debounce delay (30 ms)
     *
     * 150 ms gives plenty of margin.
     */
    k_msleep(150);


    LOG_INF("");
    LOG_INF("[FINAL] events=%d  handler_calls=%d",
            total_events,
            total_processed);


#endif


    LOG_INF("=== L3 Homework finished ===");

    return 0;
}
