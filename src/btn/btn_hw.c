#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"
#include "app_timer.h"
#include "app_util_platform.h"
#include "nrf_gpio.h"
#include "nrfx_gpiote.h"

#include "btn_hw.h"
#include "pca10059.h"

#ifndef BTN_DEBOUNCE_TIME_MS
#define BTN_DEBOUNCE_TIME_MS 10
#endif

#define BTN_DEBOUNCE_UPDATE_INTERVAL_MS 1

static const uint8_t m_btn_list[BUTTONS_NUMBER] = BUTTONS_LIST;

APP_TIMER_DEF(m_debounce_timer);

static btn_hw_callback_t m_callback;

static volatile bool m_confirmed_state;
static volatile bool m_debounce_candidate_state;
static volatile bool m_debounce_active;

static volatile uint32_t m_last_edge_tick;

/**
 * @brief Handles GPIO edge events.
 *
 * @param[in] pin GPIO pin number.
 * @param[in] action GPIOTE event polarity.
 */
static void gpiote_event_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action);

/**
 * @brief Handles debounce timer events.
 *
 * @param[in] p_context Timer context.
 */
static void debounce_timer_handler(void *p_context);

void btn_hw_init(btn_hw_callback_t callback)
{
    nrfx_gpiote_in_config_t config = NRFX_GPIOTE_CONFIG_IN_SENSE_TOGGLE(false);

    m_callback = callback;

    APP_ERROR_CHECK(app_timer_create(
        &m_debounce_timer,
        APP_TIMER_MODE_REPEATED,
        debounce_timer_handler));

    if (!nrfx_gpiote_is_init())
        APP_ERROR_CHECK(nrfx_gpiote_init());

    config.pull = BUTTON_PULL;

    APP_ERROR_CHECK(nrfx_gpiote_in_init(
        m_btn_list[0],
        &config,
        gpiote_event_handler));

    CRITICAL_REGION_ENTER();

    nrfx_gpiote_in_event_enable(m_btn_list[0], true);

    m_confirmed_state = (nrf_gpio_pin_read(m_btn_list[0]) == BUTTONS_ACTIVE_STATE);
    m_debounce_candidate_state = m_confirmed_state;
    m_debounce_active = false;

    CRITICAL_REGION_EXIT();
}

static void gpiote_event_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    (void)action;

    m_debounce_candidate_state = (nrf_gpio_pin_read(pin) == BUTTONS_ACTIVE_STATE);
    m_last_edge_tick = app_timer_cnt_get();

    if (m_debounce_active) return;

    m_debounce_active = true;

    APP_ERROR_CHECK(app_timer_start(
        m_debounce_timer,
        APP_TIMER_TICKS(BTN_DEBOUNCE_UPDATE_INTERVAL_MS),
        NULL));
}

static void debounce_timer_handler(void *p_context)
{
    bool pin_state;
    uint32_t now;

    (void)p_context;

    pin_state = (nrf_gpio_pin_read(m_btn_list[0]) == BUTTONS_ACTIVE_STATE);
    now = app_timer_cnt_get();

    if (pin_state != m_debounce_candidate_state)
    {
        m_debounce_candidate_state = pin_state;
        m_last_edge_tick = now;
        return;
    }

    if (app_timer_cnt_diff_compute(now, m_last_edge_tick) <
        APP_TIMER_TICKS(BTN_DEBOUNCE_TIME_MS)) return;

    m_debounce_active = false;

    APP_ERROR_CHECK(app_timer_stop(m_debounce_timer));

    if (m_confirmed_state == pin_state) return;

    m_confirmed_state = pin_state;

    if (m_callback != NULL)
        m_callback(pin_state);
}