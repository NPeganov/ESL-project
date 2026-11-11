#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"
#include "app_timer.h"
#include "app_util_platform.h"
#include "nrf_drv_clock.h"

#include "btn.h"
#include "btn_hw.h"

#ifndef BTN_DOUBLE_PRESS_TIME_MS
#define BTN_DOUBLE_PRESS_TIME_MS 1000
#endif

#ifndef BTN_EVENT_QUEUE_SIZE
#define BTN_EVENT_QUEUE_SIZE 8
#endif

// The timer is single-shot because the double-press interval is restarted
// explicitly after every confirmed release.
APP_TIMER_DEF(m_double_press_timer);

static btn_event_t m_event_queue[BTN_EVENT_QUEUE_SIZE];

static volatile uint8_t m_event_head;
static volatile uint8_t m_event_tail;
static volatile uint8_t m_event_count;

// True after the initial button state has been confirmed.
static bool m_state_initialized;

// True while the logical button state is pressed.
static bool m_press_started;

// True while waiting for a possible second completed press.
static bool m_double_press_pending;

static uint32_t m_first_release_tick;

static void clock_init(void);
static void event_queue_push(btn_event_t event);
static void button_state_changed(bool pressed);
static void double_press_timer_handler(void *p_context);

void btn_init(void) {
    clock_init();

    APP_ERROR_CHECK(app_timer_init());

    btn_hw_init(button_state_changed);

    APP_ERROR_CHECK(app_timer_create(
        &m_double_press_timer,
        APP_TIMER_MODE_SINGLE_SHOT,
        double_press_timer_handler));

    btn_hw_start();
}

btn_event_t btn_event_take(void) {
    btn_event_t event = BTN_EVENT_NONE;

    CRITICAL_REGION_ENTER();

    if (m_event_count > 0) {
        event = m_event_queue[m_event_tail];

        m_event_tail++;
        if (m_event_tail >= BTN_EVENT_QUEUE_SIZE)
            m_event_tail = 0;

        m_event_count--;
    }

    CRITICAL_REGION_EXIT();

    return event;
}

static void clock_init(void) {
    // Initialize the shared legacy clock driver if it is not initialized yet.
    if (!nrf_drv_clock_init_check())
        APP_ERROR_CHECK(nrf_drv_clock_init());

    // Keep LFCLK running for the button debounce timer.
    nrf_drv_clock_lfclk_request(NULL);

    while (!nrf_drv_clock_lfclk_is_running());
}

static void event_queue_push(btn_event_t event) {
    CRITICAL_REGION_ENTER();

    // Discard new events if the queue is full to preserve unread events.
    if (m_event_count < BTN_EVENT_QUEUE_SIZE) {
        m_event_queue[m_event_head] = event;

        m_event_head++;
        if (m_event_head >= BTN_EVENT_QUEUE_SIZE)
            m_event_head = 0;

        m_event_count++;
    }

    CRITICAL_REGION_EXIT();
}

static void button_state_changed(bool pressed) {
    uint32_t now;

    // The first confirmed state initializes the logical state without
    // generating events. This also handles a button held during startup.
    if (!m_state_initialized) {
        m_press_started = pressed;
        m_state_initialized = true;
        return;
    }

    if (pressed) {
        m_press_started = true;
        event_queue_push(BTN_EVENT_PRESS);
        return;
    }

    event_queue_push(BTN_EVENT_RELEASE);

    if (!m_press_started) return;

    m_press_started = false;

    // Report every completed press-release cycle.
    event_queue_push(BTN_EVENT_SINGLE_PRESS);

    now = app_timer_cnt_get();

    if (m_double_press_pending) {
        if (app_timer_cnt_diff_compute(now, m_first_release_tick) <
            APP_TIMER_TICKS(BTN_DOUBLE_PRESS_TIME_MS)) {
            m_double_press_pending = false;

            APP_ERROR_CHECK(app_timer_stop(m_double_press_timer));

            // The second completed cycle occurred within the double-press interval.
            event_queue_push(BTN_EVENT_DOUBLE_PRESS);
            return;
        }

        APP_ERROR_CHECK(app_timer_stop(m_double_press_timer));
    }

    // Start a new double-press window from this confirmed release.
    m_double_press_pending = true;
    m_first_release_tick = now;

    APP_ERROR_CHECK(app_timer_start(
        m_double_press_timer,
        APP_TIMER_TICKS(BTN_DOUBLE_PRESS_TIME_MS),
        NULL));
}

static void double_press_timer_handler(void *p_context) {
    (void)p_context;

    // The double-press window expired without a qualifying second release.
    m_double_press_pending = false;
}
