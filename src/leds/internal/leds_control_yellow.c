#include <stdbool.h>
#include <stdint.h>

#include "app_timer.h"

#include "../leds_pwm.h"
#include "leds_control_yellow.h"

// Yellow indicator intervals.
#define YELLOW_SLOW_BLINK_MS 1000
#define YELLOW_FAST_BLINK_MS 150

static lc_yellow_pattern_t pattern;
static bool state;
static uint32_t toggle_tick;

void yellow_init(void) {
    pattern = LC_YELLOW_OFF;
    state = false;
    toggle_tick = app_timer_cnt_get();

    leds_pwm_set_yellow_brightness(0);
}

bool yellow_set_pattern(lc_yellow_pattern_t new_pattern) {
    uint8_t brightness;

    switch (new_pattern) {
        case LC_YELLOW_OFF:
            state = false;
            brightness = 0;
            break;

        case LC_YELLOW_ON:
        case LC_YELLOW_SLOW_BLINK:
        case LC_YELLOW_FAST_BLINK:
            state = true;
            brightness = 100;
            break;

        default:
            return false;
    }

    pattern = new_pattern;
    toggle_tick = app_timer_cnt_get();
    leds_pwm_set_yellow_brightness(brightness);

    return true;
}

void yellow_process(uint32_t now) {
    uint32_t interval_ms;
    uint32_t interval_ticks;
    uint32_t elapsed_ticks;
    uint32_t toggle_count;

    switch (pattern) {
        case LC_YELLOW_SLOW_BLINK:
            interval_ms = YELLOW_SLOW_BLINK_MS;
            break;

        case LC_YELLOW_FAST_BLINK:
            interval_ms = YELLOW_FAST_BLINK_MS;
            break;

        default:
            return;
    }

    interval_ticks = APP_TIMER_TICKS(interval_ms);
    elapsed_ticks = app_timer_cnt_diff_compute(now, toggle_tick);
    toggle_count = elapsed_ticks / interval_ticks;

    if (toggle_count == 0) return;

    toggle_tick += toggle_count * interval_ticks;

    if (toggle_count % 2 == 0) return;

    state = !state;
    leds_pwm_set_yellow_brightness(state ? 100 : 0);
}
