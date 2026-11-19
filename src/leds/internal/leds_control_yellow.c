#include <stdbool.h>
#include <stdint.h>

#include "app_timer.h"

#include "../leds_control.h"
#include "../leds_pwm.h"
#include "leds_control_yellow.h"

// Yellow indicator intervals.
#define YELLOW_SLOW_BLINK_MS 1000
#define YELLOW_FAST_BLINK_MS 150

static leds_control_yellow_pattern_t m_pattern;
static bool m_state;
static uint32_t m_toggle_tick;

void leds_control_yellow_init(void) {
    m_pattern = LEDS_CONTROL_YELLOW_OFF;
    m_state = false;
    m_toggle_tick = app_timer_cnt_get();

    leds_pwm_set_yellow_brightness(0);
}

bool leds_control_yellow_set_pattern(
    leds_control_yellow_pattern_t pattern) {
    uint8_t brightness;

    switch (pattern) {
        case LEDS_CONTROL_YELLOW_OFF:
            m_state = false;
            brightness = 0;
            break;

        case LEDS_CONTROL_YELLOW_ON:
        case LEDS_CONTROL_YELLOW_SLOW_BLINK:
        case LEDS_CONTROL_YELLOW_FAST_BLINK:
            m_state = true;
            brightness = 100;
            break;

        default:
            return false;
    }

    m_pattern = pattern;
    m_toggle_tick = app_timer_cnt_get();
    leds_pwm_set_yellow_brightness(brightness);

    return true;
}

void leds_control_yellow_process(uint32_t now) {
    uint32_t interval_ms;
    uint32_t interval_ticks;
    uint32_t elapsed_ticks;
    uint32_t toggle_count;

    switch (m_pattern) {
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
    elapsed_ticks = app_timer_cnt_diff_compute(now, m_toggle_tick);
    toggle_count = elapsed_ticks / interval_ticks;

    if (toggle_count == 0) return;

    m_toggle_tick += toggle_count * interval_ticks;

    if (toggle_count % 2 == 0) return;

    m_state = !m_state;
    leds_pwm_set_yellow_brightness(m_state ? 100 : 0);
}
