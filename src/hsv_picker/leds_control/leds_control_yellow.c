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

// Updates the yellow LED brightness through fade and hold phases.
void yellow_process(uint32_t now) {
    uint32_t fade_in_ticks;
    uint32_t hold_on_ticks;
    uint32_t fade_out_ticks;
    uint32_t hold_off_ticks;
    uint32_t cycle_ticks;
    uint32_t phase_ticks;
    uint8_t brightness;

    switch (pattern) {
        case LC_YELLOW_SLOW_BLINK:
            fade_in_ticks = APP_TIMER_TICKS(YELLOW_SLOW_FADE_IN_MS);
            hold_on_ticks = APP_TIMER_TICKS(YELLOW_SLOW_HOLD_ON_MS);
            fade_out_ticks = APP_TIMER_TICKS(YELLOW_SLOW_FADE_OUT_MS);
            hold_off_ticks = APP_TIMER_TICKS(YELLOW_SLOW_HOLD_OFF_MS);
            break;

        case LC_YELLOW_FAST_BLINK:
            fade_in_ticks = APP_TIMER_TICKS(YELLOW_FAST_FADE_IN_MS);
            hold_on_ticks = APP_TIMER_TICKS(YELLOW_FAST_HOLD_ON_MS);
            fade_out_ticks = APP_TIMER_TICKS(YELLOW_FAST_FADE_OUT_MS);
            hold_off_ticks = APP_TIMER_TICKS(YELLOW_FAST_HOLD_OFF_MS);
            break;

        default: return;
    }

    cycle_ticks = fade_in_ticks + hold_on_ticks + fade_out_ticks + hold_off_ticks;
    phase_ticks = app_timer_cnt_diff_compute(now, pattern_start_tick) % cycle_ticks;

    if (phase_ticks < fade_in_ticks)
        brightness = (uint8_t)(phase_ticks * 100 / fade_in_ticks);
    else if ((phase_ticks -= fade_in_ticks) < hold_on_ticks)
        brightness = 100;
    else if ((phase_ticks -= hold_on_ticks) < fade_out_ticks)
        brightness = (uint8_t)(100 - phase_ticks * 100 / fade_out_ticks);
    else
        brightness = 0;

    if (brightness != leds_pwm_get_yellow_brightness())
        leds_pwm_set_yellow_brightness(brightness);
}
