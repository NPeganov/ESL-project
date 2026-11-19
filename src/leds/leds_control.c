#include <stdbool.h>
#include <stdint.h>

#include "app_timer.h"
#include "nrf_error.h"

#include "leds_control.h"
#include "leds_pwm.h"

// HSV change rates and yellow indicator intervals.
#define CONTROL_TIMER_INTERVAL_MS 10
#define HUE_CYCLE_DURATION_MS 10000
#define HSV_VALUE_HALF_CYCLE_MS 5000
#define YELLOW_SLOW_BLINK_MS 1000
#define YELLOW_FAST_BLINK_MS 150

APP_TIMER_DEF(m_control_timer);

static bool m_initialized;

static bool m_hsv_change_active;
static leds_control_hsv_parameter_t m_active_parameter;
static uint16_t m_hsv_change_start_value;
static uint32_t m_hsv_change_start_tick;

static leds_control_yellow_pattern_t m_yellow_pattern;
static bool m_yellow_state;
static uint32_t m_yellow_toggle_tick;

static void control_timer_handler(void *p_context);
static void hsv_change_process(uint32_t now);
static void yellow_process(uint32_t now);

bool leds_control_init(void) {
    if (m_initialized) return false;

    if (!leds_pwm_init()) return false;

    if (app_timer_create(
            &m_control_timer,
            APP_TIMER_MODE_REPEATED,
            control_timer_handler) != NRF_SUCCESS)
        return false;

    m_hsv_change_active = false;
    m_yellow_pattern = LEDS_CONTROL_YELLOW_OFF;
    m_yellow_state = false;
    m_yellow_toggle_tick = app_timer_cnt_get();
    m_initialized = true;

    if (app_timer_start(
            m_control_timer,
            APP_TIMER_TICKS(CONTROL_TIMER_INTERVAL_MS),
            NULL) != NRF_SUCCESS) {
        m_initialized = false;
        return false;
    }

    return true;
}

void leds_control_set_hsv(
    uint16_t hue,
    uint8_t saturation,
    uint8_t value) {
    leds_pwm_set_hsv(hue, saturation, value);
}

void leds_control_get_hsv(
    uint16_t *hue,
    uint8_t *saturation,
    uint8_t *value) {
    leds_pwm_get_hsv(hue, saturation, value);
}

bool leds_control_start_hsv_change(
    leds_control_hsv_parameter_t parameter) {
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;

    if (!m_initialized) return false;

    if (parameter != LEDS_CONTROL_HUE &&
        parameter != LEDS_CONTROL_SATURATION &&
        parameter != LEDS_CONTROL_VALUE)
        return false;

    if (m_hsv_change_active && m_active_parameter == parameter)
        return true;

    leds_pwm_get_hsv(&hue, &saturation, &value);

    switch (parameter) {
        case LEDS_CONTROL_HUE:
            m_hsv_change_start_value = hue;
            break;

        case LEDS_CONTROL_SATURATION:
            m_hsv_change_start_value = saturation;
            break;

        case LEDS_CONTROL_VALUE:
            m_hsv_change_start_value = value;
            break;

        default:
            return false;
    }

    m_active_parameter = parameter;
    m_hsv_change_start_tick = app_timer_cnt_get();
    m_hsv_change_active = true;

    return true;
}

void leds_control_stop_hsv_change(void) {
    m_hsv_change_active = false;
}

bool leds_control_set_yellow_pattern(
    leds_control_yellow_pattern_t pattern) {
    uint8_t brightness;

    if (!m_initialized) return false;

    switch (pattern) {
        case LEDS_CONTROL_YELLOW_OFF:
            m_yellow_state = false;
            brightness = 0;
            break;

        case LEDS_CONTROL_YELLOW_ON:
        case LEDS_CONTROL_YELLOW_SLOW_BLINK:
        case LEDS_CONTROL_YELLOW_FAST_BLINK:
            m_yellow_state = true;
            brightness = 100;
            break;

        default:
            return false;
    }

    m_yellow_pattern = pattern;
    m_yellow_toggle_tick = app_timer_cnt_get();
    leds_pwm_set_yellow_brightness(brightness);

    return true;
}

static void control_timer_handler(void *p_context) {
    uint32_t now = app_timer_cnt_get();

    (void)p_context;

    hsv_change_process(now);
    yellow_process(now);
}

static void hsv_change_process(uint32_t now) {
    uint32_t elapsed;
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;

    if (!m_hsv_change_active) return;

    elapsed = app_timer_cnt_diff_compute(now, m_hsv_change_start_tick);
    leds_pwm_get_hsv(&hue, &saturation, &value);

    switch (m_active_parameter) {
        case LEDS_CONTROL_HUE: {
            uint32_t cycle_ticks = APP_TIMER_TICKS(HUE_CYCLE_DURATION_MS);

            // Rotate from the current Hue at a constant rate, wrapping at 360 degrees.
            uint16_t hue_delta = (uint16_t)(
                (uint64_t)(elapsed % cycle_ticks) * 360 / cycle_ticks);
            uint16_t next_hue = (m_hsv_change_start_value + hue_delta) % 360;

            if (next_hue != hue)
                leds_pwm_set_hsv(next_hue, saturation, value);
            break;
        }

        case LEDS_CONTROL_SATURATION:
        case LEDS_CONTROL_VALUE: {
            uint32_t half_cycle_ticks = APP_TIMER_TICKS(HSV_VALUE_HALF_CYCLE_MS);
            uint32_t steps = (uint32_t)(
                (uint64_t)elapsed * 100 / half_cycle_ticks);

            // A 0..100..0 triangle wave starts by moving toward 100.
            uint16_t phase = (m_hsv_change_start_value + steps) % 200;
            uint8_t next_value = (uint8_t)(
                phase <= 100 ? phase : 200 - phase);

            if (m_active_parameter == LEDS_CONTROL_SATURATION) {
                if (next_value != saturation)
                    leds_pwm_set_hsv(hue, next_value, value);
            } else if (next_value != value)
                leds_pwm_set_hsv(hue, saturation, next_value);
            break;
        }

        default:
            m_hsv_change_active = false;
            break;
    }
}

static void yellow_process(uint32_t now) {
    uint32_t interval_ms;
    uint32_t interval_ticks;

    switch (m_yellow_pattern) {
        case LEDS_CONTROL_YELLOW_SLOW_BLINK:
            interval_ms = YELLOW_SLOW_BLINK_MS;
            break;

        case LEDS_CONTROL_YELLOW_FAST_BLINK:
            interval_ms = YELLOW_FAST_BLINK_MS;
            break;

        default:
            return;
    }

    interval_ticks = APP_TIMER_TICKS(interval_ms);

    while (app_timer_cnt_diff_compute(now, m_yellow_toggle_tick) >= interval_ticks) {
        m_yellow_toggle_tick += interval_ticks;
        m_yellow_state = !m_yellow_state;

        leds_pwm_set_yellow_brightness(m_yellow_state ? 100 : 0);
    }
}
