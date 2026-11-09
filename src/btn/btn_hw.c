#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"
#include "app_timer.h"
#include "app_util_platform.h"
#include "nrf_gpio.h"
#include "nrfx_gpiote.h"

#include "btn_hw.h"
#include "pca10059.h"

#define BTN_DEBOUNCE_TIME_MS 10

// List of buttons defined by the board configuration.
static const uint8_t m_btn_list[BUTTONS_NUMBER] = BUTTONS_LIST;

// One-shot timer used to wait until the button input
// remains stable for the complete debounce interval.
APP_TIMER_DEF(m_debounce_timer);

// Callback to the logical button layer.
static btn_hw_callback_t m_callback;

// Last button state confirmed as stable.
static volatile bool m_confirmed_state;

// Current button state observed after the latest GPIO edge.
static volatile bool m_debounce_candidate_state;

// True while the debounce timer is active.
static volatile bool m_debounce_active;

/**
 * @brief Handles GPIO edge events.
 *
 * A GPIO edge does not immediately mean that the button state
 * has changed. Mechanical contacts can produce several edges
 * while settling. Therefore, every edge starts a new debounce
 * interval.
 *
 * @param[in] pin GPIO pin number.
 * @param[in] action GPIOTE event polarity.
 */
static void gpiote_event_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action);

/**
 * @brief Handles debounce timer events.
 *
 * The timer expires after the debounce interval. The GPIO state
 * is checked again. If it differs from the candidate state, the
 * input changed during the interval and another debounce interval
 * is started. Otherwise the candidate state is considered stable
 * and becomes the confirmed state.
 *
 * @param[in] p_context Timer context.
 */
static void debounce_timer_handler(void *p_context);

/**
 * @brief Initializes button hardware handling.
 *
 * Configures GPIOTE for button edge detection, initializes the
 * debounce timer and reads the initial physical button state.
 *
 * The initial state is considered confirmed immediately, so a
 * button that is already pressed during initialization does not
 * generate a press event.
 *
 * @param[in] callback Callback for confirmed button state changes.
 */
void btn_hw_init(btn_hw_callback_t callback)
{
    nrfx_gpiote_in_config_t config = NRFX_GPIOTE_CONFIG_IN_SENSE_TOGGLE(false);

    m_callback = callback;

    APP_ERROR_CHECK(app_timer_create(
        &m_debounce_timer,
        APP_TIMER_MODE_SINGLE_SHOT,
        debounce_timer_handler));

    if (!nrfx_gpiote_is_init())
        APP_ERROR_CHECK(nrfx_gpiote_init());

    config.pull = BUTTON_PULL;

    APP_ERROR_CHECK(nrfx_gpiote_in_init(
        m_btn_list[0],
        &config,
        gpiote_event_handler));

    CRITICAL_REGION_ENTER();

    // Enable GPIO edge events before starting to monitor the button.
    nrfx_gpiote_in_event_enable(m_btn_list[0], true);

    // Establish the initial confirmed state of the button.
    m_confirmed_state =
        (nrf_gpio_pin_read(m_btn_list[0]) == BUTTONS_ACTIVE_STATE);

    // The initial state is already considered stable.
    m_debounce_candidate_state = m_confirmed_state;
    m_debounce_active = false;

    CRITICAL_REGION_EXIT();
}

static void gpiote_event_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    (void)action;

    // Remember the state observed at the latest GPIO edge.
    m_debounce_candidate_state =
        (nrf_gpio_pin_read(pin) == BUTTONS_ACTIVE_STATE);

    // If a previous debounce interval is still active,
    // cancel it because a new edge has restarted the interval.
    if (m_debounce_active)
        APP_ERROR_CHECK(app_timer_stop(m_debounce_timer));

    m_debounce_active = true;

    // Wait for the input to remain stable for the debounce interval.
    APP_ERROR_CHECK(app_timer_start(
        m_debounce_timer,
        APP_TIMER_TICKS(BTN_DEBOUNCE_TIME_MS),
        NULL));
}

static void debounce_timer_handler(void *p_context)
{
    bool pin_state;

    (void)p_context;

    // Read the physical input again after the debounce interval.
    pin_state =
        (nrf_gpio_pin_read(m_btn_list[0]) == BUTTONS_ACTIVE_STATE);

    // The input changed during the debounce interval.
    // The new state is now the candidate state, so start
    // another complete debounce interval.
    if (pin_state != m_debounce_candidate_state)
    {
        m_debounce_candidate_state = pin_state;

        APP_ERROR_CHECK(app_timer_start(
            m_debounce_timer,
            APP_TIMER_TICKS(BTN_DEBOUNCE_TIME_MS),
            NULL));

        return;
    }

    // The input remained unchanged for the complete interval.
    // The debounce procedure is therefore finished.
    m_debounce_active = false;

    // Nothing changed from the last confirmed state.
    if (m_confirmed_state == pin_state) return;

    // The new state is now confirmed.
    m_confirmed_state = pin_state;

    // Notify the logical button layer about the confirmed change.
    if (m_callback != NULL)
        m_callback(pin_state);
}