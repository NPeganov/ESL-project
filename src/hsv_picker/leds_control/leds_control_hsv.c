#include <stdbool.h>
#include <stdint.h>

#include "app_timer.h"

#include "../../leds/leds_pwm.h"
#include "leds_control_hsv.h"

// HSV change rates.
#define HUE_CYCLE_DURATION_MS 10000
#define HSV_VALUE_HALF_CYCLE_MS 5000

typedef enum {
    CHANGE_IDLE,
    CHANGE_RUNNING,
    CHANGE_PAUSED
} change_state_t;

static change_state_t change_state;             // Current change lifecycle state.
static lc_hsv_parameter_t active_parameter;     // HSV component being changed.
static uint16_t start_value;                    // Component value when this run began.
static uint32_t elapsed_ticks;                  // Accumulated active time, excluding pauses.
static uint32_t last_tick;                      // Timestamp of the previous update.

void hsv_init(void) {
    change_state = CHANGE_IDLE;
}

bool hsv_start(lc_hsv_parameter_t parameter) {
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;
    uint32_t now;

    if (parameter != LC_HUE &&
        parameter != LC_SATURATION &&
        parameter != LC_VALUE)
        return false;

    if (change_state == CHANGE_RUNNING && active_parameter == parameter)
        return true;

    now = app_timer_cnt_get();

    // Resume the same parameter without changing its position or direction.
    if (change_state == CHANGE_PAUSED && active_parameter == parameter) {
        last_tick = now;
        change_state = CHANGE_RUNNING;
        return true;
    }

    leds_pwm_get_hsv(&hue, &saturation, &value);

    switch (parameter) {
        case LC_HUE: start_value = hue; break;
        case LC_SATURATION: start_value = saturation; break;
        case LC_VALUE: start_value = value; break;
    }

    active_parameter = parameter;
    elapsed_ticks = 0;
    last_tick = now;
    change_state = CHANGE_RUNNING;

    return true;
}

void hsv_stop(void) {
    if (change_state != CHANGE_RUNNING) return;

    // Account for time since the last timer callback before pausing.
    elapsed_ticks += app_timer_cnt_diff_compute(
        app_timer_cnt_get(),
        last_tick);

    change_state = CHANGE_PAUSED;
}

void hsv_process(uint32_t now) {
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;
    uint16_t next_hue;
    uint8_t next_saturation;
    uint8_t next_value;

    if (change_state != CHANGE_RUNNING) return;

    elapsed_ticks += app_timer_cnt_diff_compute(now, last_tick);
    last_tick = now;

    leds_pwm_get_hsv(&hue, &saturation, &value);

    next_hue = hue;
    next_saturation = saturation;
    next_value = value;

    switch (active_parameter) {
        case LC_HUE: {
            uint32_t cycle_ticks = APP_TIMER_TICKS(HUE_CYCLE_DURATION_MS);

            // Calculate Hue from the original value and total active time to avoid drift.
            uint16_t hue_delta = (uint16_t)(
                (uint64_t)(elapsed_ticks % cycle_ticks) *
                360 / cycle_ticks);

            next_hue = (start_value + hue_delta) % 360;
            break;
        }

        case LC_SATURATION:
        case LC_VALUE: {
            uint32_t half_cycle_ticks = APP_TIMER_TICKS(HSV_VALUE_HALF_CYCLE_MS);
            uint32_t steps = (uint32_t)(
                (uint64_t)elapsed_ticks * 100 /
                half_cycle_ticks);

            // The triangle wave moves 0..100..0, starting toward 100 from the initial value.
            uint16_t phase = (start_value + steps) % 200;
            uint8_t next_component = (uint8_t)(
                phase <= 100 ? phase : 200 - phase);

            if (active_parameter == LC_SATURATION) next_saturation = next_component;
            else next_value = next_component;
            break;
        }

        // A running change must always have a valid active parameter.
        default: change_state = CHANGE_IDLE; return;
    }

    // Avoid unnecessary writes when the calculated color has not changed.
    if (next_hue != hue ||
        next_saturation != saturation ||
        next_value != value)
        leds_pwm_set_hsv(next_hue, next_saturation, next_value);
}
