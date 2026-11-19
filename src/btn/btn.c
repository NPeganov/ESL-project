#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"
#include "app_timer.h"
#include "nrf_assert.h"

#include "btn.h"
#include "btn_hw.h"

#ifndef BTN_DOUBLE_PRESS_TIME_MS
#define BTN_DOUBLE_PRESS_TIME_MS 1000
#endif

// The timer is single-shot because the double-press interval is restarted
// explicitly after every confirmed release.
APP_TIMER_DEF(m_double_press_timer);

static btn_event_handler_t m_event_handler;

// True after the initial button state has been confirmed.
static bool m_state_initialized;

// True while the logical button state is pressed.
static bool m_press_started;

// True while waiting for a possible second completed press.
static bool m_double_press_pending;

static uint32_t m_first_release_tick;

static void button_state_changed(bool pressed);
static void event_notify(btn_event_t event);
static void double_press_timer_handler(void *p_context);

void btn_init(btn_event_handler_t event_handler) {
    ASSERT(event_handler != NULL);

    m_event_handler = event_handler;

    btn_hw_init(button_state_changed);

    APP_ERROR_CHECK(app_timer_create(
        &m_double_press_timer,
        APP_TIMER_MODE_SINGLE_SHOT,
        double_press_timer_handler));
}

void btn_start(void) {
    btn_hw_start();
}

static void button_state_changed(bool pressed) {
    uint32_t now;

    // This callback runs in interrupt context. Logging must remain deferred
    // so that the callback does not perform synchronous backend I/O.

    // The first confirmed state initializes the logical state without
    // generating events. This also handles a button held during startup.
    if (!m_state_initialized) {
        m_press_started = pressed;
        m_state_initialized = true;
        return;
    }

    if (pressed) {
        m_press_started = true;

        event_notify(BTN_EVENT_PRESS);
        return;
    }

    event_notify(BTN_EVENT_RELEASE);

    if (!m_press_started) return;

    m_press_started = false;

    // Report every completed press-release cycle.
    event_notify(BTN_EVENT_SINGLE_PRESS);

    now = app_timer_cnt_get();

    if (m_double_press_pending) {
        if (app_timer_cnt_diff_compute(now, m_first_release_tick) <
            APP_TIMER_TICKS(BTN_DOUBLE_PRESS_TIME_MS)) {
            m_double_press_pending = false;

            APP_ERROR_CHECK(app_timer_stop(m_double_press_timer));

            // The second completed cycle occurred within the double-press interval.
            event_notify(BTN_EVENT_DOUBLE_PRESS);
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

static void event_notify(btn_event_t event) {
    m_event_handler(event);
}

static void double_press_timer_handler(void *p_context) {
    (void)p_context;

    // The double-press window expired without a qualifying second release.
    m_double_press_pending = false;
}
