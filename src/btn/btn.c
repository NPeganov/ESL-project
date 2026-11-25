#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"
#include "app_timer.h"
#include "nrf_assert.h"

#include "btn.h"
#include "btn_hw.h"

#define BTN_DOUBLE_PRESS_TIME_MS 1000
#define BTN_HOLD_TIME_MS 250

// Single-shot timers for the double-press window and hold threshold.
APP_TIMER_DEF(m_double_press_timer);
APP_TIMER_DEF(m_hold_timer);

static btn_event_handler_t m_event_handler;

static bool m_initial_stable_state_pending;  // True until the initial stable button state is received.
static bool m_press_detected;                // True after a press has been detected and until its release.
static bool m_wait_for_second_press;         // True while waiting for a second single press.
static bool m_hold_detected;                 // True after HOLD has been reported for the current press cycle.
static uint32_t m_first_release_tick;        // Tick count of the first release in the current double-press window.

static void button_state_changed(bool pressed);
static void process_button_state(bool pressed);
static void handle_button_press(void);
static void handle_button_release(void);
static void proceed_with_single_press(void);
static void hold_timer_handler(void *p_context);
static void double_press_timer_handler(void *p_context);

void btn_init(btn_event_handler_t event_handler) {
    ASSERT(event_handler != NULL);

    m_event_handler = event_handler;
    m_initial_stable_state_pending = true;

    btn_hw_init(button_state_changed);

    APP_ERROR_CHECK(app_timer_create(
        &m_double_press_timer,
        APP_TIMER_MODE_SINGLE_SHOT,
        double_press_timer_handler));

    APP_ERROR_CHECK(app_timer_create(
        &m_hold_timer,
        APP_TIMER_MODE_SINGLE_SHOT,
        hold_timer_handler));

    // Enable button events only after the callback and timers are ready.
    btn_hw_start();
}

// Process a confirmed button state change.
static void button_state_changed(bool pressed) {
    // This callback runs in interrupt context. Logging must remain deferred
    // so that the callback does not perform synchronous backend I/O.

    // Ignore the initial stable button state; it must not generate a button event.
    if (m_initial_stable_state_pending) {
        m_initial_stable_state_pending = false;
        return;
    }

    process_button_state(pressed);
}

// Dispatch a confirmed state change to the corresponding handler.
static void process_button_state(bool pressed) {
    if (pressed) handle_button_press();
    else handle_button_release();
}

// Start processing a new press cycle.
static void handle_button_press(void) {
    m_press_detected = true;
    m_hold_detected = false;

    APP_ERROR_CHECK(app_timer_start(
        m_hold_timer,
        APP_TIMER_TICKS(BTN_HOLD_TIME_MS),
        NULL));

    m_event_handler(BTN_EVENT_PRESS);
}

// Finish processing the current press cycle on release.
static void handle_button_release(void) {
    // Ignore a release unless this module has reported the corresponding press.
    if (!m_press_detected) return;

    m_event_handler(BTN_EVENT_RELEASE);

    m_press_detected = false;

    APP_ERROR_CHECK(app_timer_stop(m_hold_timer));

    // After a hold, do not classify this release as a single press.
    // Otherwise, continue single/double press detection.
    if (m_hold_detected) m_hold_detected = false;
    else proceed_with_single_press();
}

// Interpret a completed single press and update double-press detection.
static void proceed_with_single_press(void) {
    m_event_handler(BTN_EVENT_SINGLE_PRESS);

    uint32_t now = app_timer_cnt_get();

    // If a previous single press is pending, the current one either completes
    // a double press or starts a new double-press window.
    if (m_wait_for_second_press) {
        // The current release completes the previous double-press window.
        APP_ERROR_CHECK(app_timer_stop(m_double_press_timer));

        // Check whether the current release occurred within the allowed interval.
        if (app_timer_cnt_diff_compute(now, m_first_release_tick) <
            APP_TIMER_TICKS(BTN_DOUBLE_PRESS_TIME_MS)) {
            m_wait_for_second_press = false;

            // A second single press was detected within the double-press interval.
            m_event_handler(BTN_EVENT_DOUBLE_PRESS);
            return;
        }
    }

    // No qualifying second single press was detected, so the current release
    // starts a new double-press window.
    m_wait_for_second_press = true;
    m_first_release_tick = now;

    APP_ERROR_CHECK(app_timer_start(
        m_double_press_timer,
        APP_TIMER_TICKS(BTN_DOUBLE_PRESS_TIME_MS),
        NULL));
}

// Report a hold when the hold timer expires.
static void hold_timer_handler(void *p_context) {
    (void)p_context;

    // Ignore the timeout if the button has already been released.
    if (!m_press_detected) return;

    m_hold_detected = true;

    // Stop the double-press timer if a candidate is pending.
    if (m_wait_for_second_press)
        APP_ERROR_CHECK(app_timer_stop(m_double_press_timer));

    m_wait_for_second_press = false;

    m_event_handler(BTN_EVENT_HOLD);
}

// Clear the pending double-press state when its timer expires.
static void double_press_timer_handler(void *p_context) {
    (void)p_context;

    // The double-press window expired without a qualifying second single press.
    m_wait_for_second_press = false;
}
