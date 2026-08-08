#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(demo, LOG_LEVEL_INF);

#define STACK_SIZE 1024

/* Thread priorities */
#define PRIO_COOP -1
#define PRIO_HIGH  3
#define PRIO_MED   5
#define PRIO_LOW   7


/*
 * Low priority preemptive thread
 *
 * Priority: 7
 * Runs and then sleeps for 300 ms.
 */
void t_low_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        LOG_INF("T_LOW running");

        k_msleep(300);
    }
}


/*
 * Medium priority preemptive thread
 *
 * Priority: 5
 * Runs and then sleeps for 200 ms.
 */
void t_med_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        LOG_INF("T_MED running");

        k_msleep(200);
    }
}


/*
 * High priority preemptive thread
 *
 * Priority: 3
 * Runs and then sleeps for 100 ms.
 */
void t_high_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {
        LOG_INF("T_HIGH running");

        k_msleep(100);
    }
}


/*
 * Cooperative thread
 *
 * Priority: -1
 *
 * Performs 5 iterations of busy work and then yields.
 */
void t_coop_fn(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    while (1) {

        LOG_INF("T_COOP starting busy work");

        for (int i = 0; i < 5; i++) {

            volatile uint32_t work = 0;

            for (uint32_t j = 0; j < 10000; j++) {
                work += j;
            }

            ARG_UNUSED(work);

            LOG_INF("T_COOP busy iteration %d", i + 1);
        }

        LOG_INF("T_COOP yielding");

        k_yield();
    }
}


/*
 * Thread definitions
 */

K_THREAD_DEFINE(
    t_low,
    STACK_SIZE,
    t_low_fn,
    NULL,
    NULL,
    NULL,
    PRIO_LOW,
    0,
    0
);

K_THREAD_DEFINE(
    t_med,
    STACK_SIZE,
    t_med_fn,
    NULL,
    NULL,
    NULL,
    PRIO_MED,
    0,
    0
);

K_THREAD_DEFINE(
    t_high,
    STACK_SIZE,
    t_high_fn,
    NULL,
    NULL,
    NULL,
    PRIO_HIGH,
    0,
    0
);

K_THREAD_DEFINE(
    t_coop,
    STACK_SIZE,
    t_coop_fn,
    NULL,
    NULL,
    NULL,
    PRIO_COOP,
    0,
    0
);


/*
 * Main application
 */
int main(void)
{
    LOG_INF("Starting thread scheduling task");

    return 0;
}

