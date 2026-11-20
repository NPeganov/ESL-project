#include <stdbool.h>
#include <stdint.h>

#include "app_timer.h"

#include "../../leds/leds_pwm.h"
#include "leds_control_yellow.h"

// Slow blink phase durations.
#define YELLOW_SLOW_FADE_IN_MS  1000
#define YELLOW_SLOW_HOLD_ON_MS  200
#define YELLOW_SLOW_FADE_OUT_MS 1000
#define YELLOW_SLOW_HOLD_OFF_MS 200

// Fast blink phase durations.
#define YELLOW_FAST_FADE_IN_MS  150
#define YELLOW_FAST_HOLD_ON_MS  50
#define YELLOW_FAST_FADE_OUT_MS 150
#define YELLOW_FAST_HOLD_OFF_MS 50

typedef struct {
    uint32_t fade_in_ticks;
    uint32_t hold_on_ticks;
    uint32_t fade_out_ticks;
    uint32_t hold_off_ticks;
    uint32_t cycle_ticks;
} blink_timing_t;

static lc_yellow_pattern_t pattern;       // Selected indication pattern.
static blink_timing_t timing;             // Tick durations for the selected blink pattern.
static uint32_t pattern_start_tick;       // Timestamp when the current pattern started.

void yellow_init(void) {
    pattern = LC_YELLOW_OFF;
}

// Applies a yellow LED pattern and resets its timing when blinking starts.
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
            timing.fade_in_ticks = APP_TIMER_TICKS(YELLOW_SLOW_FADE_IN_MS);
            timing.hold_on_ticks = APP_TIMER_TICKS(YELLOW_SLOW_HOLD_ON_MS);
            timing.fade_out_ticks = APP_TIMER_TICKS(YELLOW_SLOW_FADE_OUT_MS);
            timing.hold_off_ticks = APP_TIMER_TICKS(YELLOW_SLOW_HOLD_OFF_MS);
            brightness = 0;
            break;

        case LC_YELLOW_FAST_BLINK:
            timing.fade_in_ticks = APP_TIMER_TICKS(YELLOW_FAST_FADE_IN_MS);
            timing.hold_on_ticks = APP_TIMER_TICKS(YELLOW_FAST_HOLD_ON_MS);
            timing.fade_out_ticks = APP_TIMER_TICKS(YELLOW_FAST_FADE_OUT_MS);
            timing.hold_off_ticks = APP_TIMER_TICKS(YELLOW_FAST_HOLD_OFF_MS);
            brightness = 0;
            break;

        default: return false;
    }

    pattern = new_pattern;

    if (new_pattern == LC_YELLOW_SLOW_BLINK ||
        new_pattern == LC_YELLOW_FAST_BLINK) {
        timing.cycle_ticks = timing.fade_in_ticks +
                             timing.hold_on_ticks +
                             timing.fade_out_ticks +
                             timing.hold_off_ticks;
        pattern_start_tick = app_timer_cnt_get();
    }

    leds_pwm_set_yellow_brightness(brightness);

    return true;
}

// Calculates brightness for the current position within a blink cycle.
static uint8_t get_blink_brightness(uint32_t phase_ticks) {
    uint32_t fade_in_end = timing.fade_in_ticks;
    uint32_t hold_on_end = fade_in_end + timing.hold_on_ticks;
    uint32_t fade_out_end = hold_on_end + timing.fade_out_ticks;

    if (phase_ticks < fade_in_end)
        return (uint8_t)(phase_ticks * 100 / timing.fade_in_ticks);

    if (phase_ticks < hold_on_end)
        return 100;

    if (phase_ticks < fade_out_end) {
        phase_ticks -= hold_on_end;
        return (uint8_t)(100 - phase_ticks * 100 / timing.fade_out_ticks);
    }

    return 0;
}

// Updates the yellow LED brightness through fade and hold phases.
void yellow_process(uint32_t now) {
    uint32_t phase_ticks;
    uint8_t brightness;

    if (pattern != LC_YELLOW_SLOW_BLINK &&
        pattern != LC_YELLOW_FAST_BLINK)
        return;

    phase_ticks = app_timer_cnt_diff_compute(now, pattern_start_tick) %
                  timing.cycle_ticks;
    brightness = get_blink_brightness(phase_ticks);

    if (brightness != leds_pwm_get_yellow_brightness())
        leds_pwm_set_yellow_brightness(brightness);
}
