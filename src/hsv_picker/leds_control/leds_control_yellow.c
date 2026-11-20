#include <stdbool.h>
#include <stdint.h>

#include "app_timer.h"

#include "../../leds/leds_pwm.h"
#include "leds_control_yellow.h"

// Yellow LED fade durations.
#define YELLOW_SLOW_BLINK_MS 1000
#define YELLOW_FAST_BLINK_MS 150

static lc_yellow_pattern_t pattern;    // Selected indication pattern.
static uint32_t pattern_start_tick;    // Timestamp when the current pattern started.

void yellow_init(void) {
    pattern = LC_YELLOW_OFF;
}

// Sets the initial output brightness and restarts the pattern timing.
bool yellow_set_pattern(lc_yellow_pattern_t new_pattern) {
    uint8_t brightness;

    switch (new_pattern) {
        case LC_YELLOW_OFF:
            brightness = 0;
            break;

        case LC_YELLOW_ON:
            brightness = 100;
            break;

        case LC_YELLOW_SLOW_BLINK:
        case LC_YELLOW_FAST_BLINK:
            brightness = 0;
            break;

        default: return false;
    }

    pattern = new_pattern;
    pattern_start_tick = app_timer_cnt_get();
    leds_pwm_set_yellow_brightness(brightness);

    return true;
}

// Updates the yellow LED brightness along a triangular fade cycle.
void yellow_process(uint32_t now) {
    uint32_t interval_ms;
    uint32_t interval_ticks;
    uint32_t cycle_ticks;
    uint32_t phase_ticks;
    uint8_t brightness;

    switch (pattern) {
        case LC_YELLOW_SLOW_BLINK: interval_ms = YELLOW_SLOW_BLINK_MS; break;
        case LC_YELLOW_FAST_BLINK: interval_ms = YELLOW_FAST_BLINK_MS; break;
        default: return;
    }

    interval_ticks = APP_TIMER_TICKS(interval_ms);
    cycle_ticks = interval_ticks * 2;
    phase_ticks = app_timer_cnt_diff_compute(now, pattern_start_tick) % cycle_ticks;

    if (phase_ticks <= interval_ticks)
        brightness = (uint8_t)(phase_ticks * 100 / interval_ticks);
    else
        brightness = (uint8_t)((cycle_ticks - phase_ticks) * 100 / interval_ticks);

    if (brightness != leds_pwm_get_yellow_brightness())
        leds_pwm_set_yellow_brightness(brightness);
}
