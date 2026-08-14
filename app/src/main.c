#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(home_task, LOG_LEVEL_INF);

#define STACK_SIZE      1024
#define THREAD_PRIORITY 5
#define NUM_INCREMENTS  1000

/*
 * Home Task - Synchronization & Shared-State Safety
 *
 * The task asks for:
 *   1. Two equal-priority threads.
 *   2. Both threads increment the same shared counter.
 *   3. Observe the race condition.
 *   4. Fix the race using a mutex.
 *
 * Set USE_MUTEX to 0 to reproduce the race condition.
 * Set USE_MUTEX to 1 for the corrected version.
 *
 * The submitted/fixed version uses USE_MUTEX = 1.
 */
#define USE_MUTEX 1

static int shared_counter = 0;
static struct k_mutex counter_mutex;

/*
 * Each thread performs a read -> modify -> write operation.
 *
 * Without the mutex, k_yield() between the read and write makes it
 * possible for the other thread to read the same old value. One
 * increment is then lost.
 */
static void counter_thread(void *p1, void *p2, void *p3)
{
    const char *name = (const char *)p1;

    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    for (int i = 0; i < NUM_INCREMENTS; i++) {

#if USE_MUTEX
        k_mutex_lock(&counter_mutex, K_FOREVER);
#endif

        /* Explicit read -> modify -> write sequence. */
        int value = shared_counter;

#if !USE_MUTEX
        /* Deliberately expose the race between read and write. */
        k_yield();
#endif

        value++;
        shared_counter = value;

#if USE_MUTEX
        k_mutex_unlock(&counter_mutex);
#endif

        /* Yield outside the critical section. */
        if ((i % 10) == 0) {
            k_yield();
        }
    }

    LOG_INF("%s finished", name);
}

/* Two threads with exactly the same priority. */
K_THREAD_DEFINE(
    counter_thread_a,
    STACK_SIZE,
    counter_thread,
    "T_A",
    NULL,
    NULL,
    THREAD_PRIORITY,
    0,
    0
);

K_THREAD_DEFINE(
    counter_thread_b,
    STACK_SIZE,
    counter_thread,
    "T_B",
    NULL,
    NULL,
    THREAD_PRIORITY,
    0,
    0
);

int main(void)
{
    LOG_INF("========================================");
    LOG_INF("L2 Task 1 - Shared Counter");
    LOG_INF("Two equal-priority threads");
#if USE_MUTEX
    LOG_INF("Synchronization: MUTEX ENABLED");
#else
    LOG_INF("Synchronization: MUTEX DISABLED");
    LOG_INF("Race condition demonstration");
#endif
    LOG_INF("Expected final counter: %d", 2 * NUM_INCREMENTS);
    LOG_INF("========================================");

    k_mutex_init(&counter_mutex);

    /* Wait until both worker threads have completed. */
    k_thread_join(counter_thread_a, K_FOREVER);
    k_thread_join(counter_thread_b, K_FOREVER);

    LOG_INF("----------------------------------------");
    LOG_INF("Final counter = %d", shared_counter);
    LOG_INF("Expected      = %d", 2 * NUM_INCREMENTS);

    if (shared_counter == (2 * NUM_INCREMENTS)) {
        LOG_INF("RESULT: PASS - no increments were lost");
    } else {
        LOG_INF("RESULT: RACE CONDITION - increments were lost");
    }

    LOG_INF("----------------------------------------");

    return 0;
}

