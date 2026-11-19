#include <stdbool.h>
#include <stdint.h>

#include "app_timer.h"

#include "../leds_control.h"
#include "../leds_pwm.h"
#include "leds_control_hsv.h"

// HSV change rates.
#define HUE_CYCLE_DURATION_MS 10000
#define HSV_VALUE_HALF_CYCLE_MS 5000

static bool m_change_active;
static bool m_change_paused;
static leds_control_hsv_parameter_t m_active_parameter;
static uint16_t m_start_value;
static uint32_t m_elapsed_ticks;
static uint32_t m_last_tick;

void leds_control_hsv_init(void) {
    m_change_active = false;
    m_change_paused = false;
}

bool leds_control_hsv_start(leds_control_hsv_parameter_t parameter) {
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;
    uint32_t now;

    if (parameter != LEDS_CONTROL_HUE &&
        parameter != LEDS_CONTROL_SATURATION &&
        parameter != LEDS_CONTROL_VALUE)
        return false;

    if (m_change_active && m_active_parameter == parameter)
        return true;

    now = app_timer_cnt_get();

    // Resume the same parameter without changing its position or direction.
    if (m_change_paused && m_active_parameter == parameter) {
        m_last_tick = now;
        m_change_active = true;
        m_change_paused = false;
        return true;
    }

    leds_pwm_get_hsv(&hue, &saturation, &value);

    switch (parameter) {
        case LEDS_CONTROL_HUE:
            m_start_value = hue;
            break;

        case LEDS_CONTROL_SATURATION:
            m_start_value = saturation;
            break;

        case LEDS_CONTROL_VALUE:
            m_start_value = value;
            break;

        default:
            return false;
    }

    m_active_parameter = parameter;
    m_elapsed_ticks = 0;
    m_last_tick = now;
    m_change_active = true;
    m_change_paused = false;

    return true;
}

void leds_control_hsv_stop(void) {
    if (!m_change_active) return;

    m_elapsed_ticks += app_timer_cnt_diff_compute(
        app_timer_cnt_get(),
        m_last_tick);

    m_change_active = false;
    m_change_paused = true;
}

void leds_control_hsv_process(uint32_t now) {
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;

    if (!m_change_active) return;

    m_elapsed_ticks += app_timer_cnt_diff_compute(now, m_last_tick);
    m_last_tick = now;

    leds_pwm_get_hsv(&hue, &saturation, &value);

    switch (m_active_parameter) {
        case LEDS_CONTROL_HUE: {
            uint32_t cycle_ticks = APP_TIMER_TICKS(HUE_CYCLE_DURATION_MS);

            // Rotate from the current Hue at a constant rate, wrapping at 360 degrees.
            uint16_t hue_delta = (uint16_t)(
                (uint64_t)(m_elapsed_ticks % cycle_ticks) *
                360 / cycle_ticks);
            uint16_t next_hue = (m_start_value + hue_delta) % 360;

            if (next_hue != hue)
                leds_pwm_set_hsv(next_hue, saturation, value);
            break;
        }

        case LEDS_CONTROL_SATURATION:
        case LEDS_CONTROL_VALUE: {
            uint32_t half_cycle_ticks = APP_TIMER_TICKS(HSV_VALUE_HALF_CYCLE_MS);
            uint32_t steps = (uint32_t)(
                (uint64_t)m_elapsed_ticks * 100 /
                half_cycle_ticks);

            // A 0..100..0 triangle wave starts by moving toward 100.
            uint16_t phase = (m_start_value + steps) % 200;
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
            m_change_active = false;
            m_change_paused = false;
            break;
    }
}
