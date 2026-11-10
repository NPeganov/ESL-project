#include <stdbool.h>

#include "app_error.h"
#include "app_timer.h"
#include "app_util_platform.h"
#include "nrf_gpio.h"
#include "nrfx_gpiote.h"

#include "btn_hw.h"
#include "pca10059.h"

#define BTN_DEBOUNCE_TIME_MS 10

// Application button GPIO pin.
static const nrfx_gpiote_pin_t m_btn_pin = BUTTON_1;

// One-shot timer used to wait until the button input remains stable.
APP_TIMER_DEF(m_debounce_timer);

// Callback to the logical button layer.
static btn_hw_callback_t m_callback;

// Last button state confirmed as stable.
static volatile bool m_confirmed_state;

// State observed after the latest GPIO edge.
static volatile bool m_debounce_candidate_state;

// True until the initial button state has been confirmed.
static volatile bool m_initial_state_pending;

static void gpiote_pin_change_callback(
    nrfx_gpiote_pin_t pin,
    nrf_gpiote_polarity_t action);
    
static void rtc1_debounce_callback(void *p_context);

void btn_hw_init(btn_hw_callback_t callback) {
    nrfx_gpiote_in_config_t config = NRFX_GPIOTE_CONFIG_IN_SENSE_TOGGLE(false);

    m_callback = callback;

    // Create the debounce timer. LFCLK and app_timer are initialized by btn_init().
    APP_ERROR_CHECK(app_timer_create(
        &m_debounce_timer,
        APP_TIMER_MODE_SINGLE_SHOT,
        rtc1_debounce_callback));

    // GPIOTE is shared globally, so reuse an existing initialization.
    if (!nrfx_gpiote_is_init()) APP_ERROR_CHECK(nrfx_gpiote_init());

    config.pull = BUTTON_PULL;

    APP_ERROR_CHECK(nrfx_gpiote_in_init(
        m_btn_pin,
        &config,
        gpiote_pin_change_callback));
}

void btn_hw_start(void) {
    CRITICAL_REGION_ENTER();

    // Enable GPIO edge events before reading the initial state.
    nrfx_gpiote_in_event_enable(m_btn_pin, true);

    // Establish the initial debounce candidate.
    m_debounce_candidate_state = (nrf_gpio_pin_read(m_btn_pin) == BUTTONS_ACTIVE_STATE);

    m_initial_state_pending = true;

    // The initial state must remain stable for the complete debounce interval.
    APP_ERROR_CHECK(app_timer_start(
        m_debounce_timer,
        APP_TIMER_TICKS(BTN_DEBOUNCE_TIME_MS),
        NULL));

    CRITICAL_REGION_EXIT();
}

static void gpiote_pin_change_callback(
    nrfx_gpiote_pin_t pin,
    nrf_gpiote_polarity_t action) {
    (void)action;

    // Remember the state observed at the latest GPIO edge.
    m_debounce_candidate_state = (nrf_gpio_pin_read(pin) == BUTTONS_ACTIVE_STATE);

    // Every edge restarts the debounce interval.
    APP_ERROR_CHECK(app_timer_stop(m_debounce_timer));

    APP_ERROR_CHECK(app_timer_start(
        m_debounce_timer,
        APP_TIMER_TICKS(BTN_DEBOUNCE_TIME_MS),
        NULL));
}

static void rtc1_debounce_callback(void *p_context) {
    bool pin_state;

    (void)p_context;

    // Check the physical input again after the debounce interval.
    pin_state = (nrf_gpio_pin_read(m_btn_pin) == BUTTONS_ACTIVE_STATE);

    // If the input changed during the interval, wait for another complete interval.
    if (pin_state != m_debounce_candidate_state) {
        m_debounce_candidate_state = pin_state;

        APP_ERROR_CHECK(app_timer_start(
            m_debounce_timer,
            APP_TIMER_TICKS(BTN_DEBOUNCE_TIME_MS),
            NULL));

        return;
    }

    // The initial state is reported only after it has passed debounce.
    if (m_initial_state_pending) {
        m_confirmed_state = pin_state;
        m_initial_state_pending = false;

        if (m_callback != NULL)
            m_callback(pin_state);

        return;
    }

    // Ignore the callback if the confirmed state has not changed.
    if (m_confirmed_state == pin_state) return;

    m_confirmed_state = pin_state;

    if (m_callback != NULL) m_callback(pin_state);
}