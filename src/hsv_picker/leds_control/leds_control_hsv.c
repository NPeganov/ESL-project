#include <stdbool.h>
#include <stdint.h>

#include "app_timer.h"

#include "../../tools/leds/leds_pwm.h"
#include "leds_control_hsv.h"

// Time for one complete Hue rotation and for one Saturation/Value half-cycle.
#define HUE_CYCLE_DURATION_MS       10000
#define SATURATION_HALF_CYCLE_MS     5000
#define VALUE_HALF_CYCLE_MS          5000

typedef enum {
    CHANGE_IDLE,       // No parameter is currently being changed.
    CHANGE_RUNNING,    // The selected parameter is changing.
    CHANGE_PAUSED      // Progress is preserved until the same parameter resumes.
} change_state_t;

static change_state_t change_state;             // Current change lifecycle state.
static lc_change_parameter_t active_parameter;  // HSV component being changed.
static uint16_t start_value;                    // Component value when this run began.
static uint32_t elapsed_ticks;                  // Active time accumulated, excluding pauses.
static uint32_t last_tick;                      // Timestamp of the previous update.

// Resets the HSV change controller to its idle state.
void hsv_init(void) {
    change_state = CHANGE_IDLE;
}

// Starts a new parameter change or resumes the same paused change.
void hsv_start(lc_change_parameter_t parameter) {
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;
    uint32_t now;

    // Repeated start requests must not reset an already running change.
    if (change_state == CHANGE_RUNNING && active_parameter == parameter) return;

    now = app_timer_cnt_get();

    // Resume from the saved progress and exclude the time spent paused.
    if (change_state == CHANGE_PAUSED && active_parameter == parameter) {
        last_tick = now;
        change_state = CHANGE_RUNNING;
        return;
    }

    // Apply the old parameter's elapsed time before switching to another one.
    if (change_state == CHANGE_RUNNING) hsv_process(now);

    leds_pwm_get_hsv(&hue, &saturation, &value);

    // Save the selected component's current value as the origin of its new cycle.
    switch (parameter) {
        case LC_HUE: start_value = hue; break;
        case LC_SATURATION: start_value = saturation; break;
        case LC_VALUE: start_value = value; break;
    }

    active_parameter = parameter;
    elapsed_ticks = 0;
    last_tick = now;
    change_state = CHANGE_RUNNING;
}

// Applies the last elapsed interval, then pauses without losing cycle progress.
void hsv_stop(void) {
    if (change_state != CHANGE_RUNNING) return;

    hsv_process(app_timer_cnt_get());
    change_state = CHANGE_PAUSED;
}

// Advances the active parameter using the elapsed time since the previous update.
void hsv_process(uint32_t now) {
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;
    uint16_t next_hue;
    uint8_t next_saturation;
    uint8_t next_value;

    if (change_state != CHANGE_RUNNING) return;

    // Accumulate only active time; this also handles timer counter wraparound.
    elapsed_ticks += app_timer_cnt_diff_compute(now, last_tick);
    last_tick = now;

    leds_pwm_get_hsv(&hue, &saturation, &value);

    next_hue = hue;
    next_saturation = saturation;
    next_value = value;

    switch (active_parameter) {
        case LC_HUE: {
            uint32_t cycle_ticks = APP_TIMER_TICKS(HUE_CYCLE_DURATION_MS);

            // Derive Hue from the original value and elapsed time to avoid drift.
            uint16_t hue_delta = (uint16_t)(
                (uint64_t)(elapsed_ticks % cycle_ticks) *
                360 / cycle_ticks);

            next_hue = (start_value + hue_delta) % 360;
            break;
        }

        case LC_SATURATION:
        case LC_VALUE: {
            uint32_t half_cycle_ms = SATURATION_HALF_CYCLE_MS;

            if (active_parameter == LC_VALUE)
                half_cycle_ms = VALUE_HALF_CYCLE_MS;

            uint32_t half_cycle_ticks = APP_TIMER_TICKS(half_cycle_ms);
            uint32_t steps = (uint32_t)(
                (uint64_t)elapsed_ticks * 100 /
                half_cycle_ticks);

            // Triangle wave: move from the initial value toward 100, then toward 0.
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

    // Update the LED only when the calculated HSV color has actually changed.
    if (next_hue != hue ||
        next_saturation != saturation ||
        next_value != value)
        leds_pwm_set_hsv(next_hue, next_saturation, next_value);
}
