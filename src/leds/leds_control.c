#include <stdbool.h>
#include <stdint.h>

#include "app_timer.h"
#include "nrf_error.h"

#include "leds_control.h"
#include "leds_pwm.h"
#include "leds/internal/leds_control_hsv.h"
#include "leds/internal/leds_control_yellow.h"

#define CONTROL_TIMER_INTERVAL_MS 10

APP_TIMER_DEF(control_timer);

static bool initialized;

static void timer_handler(void *p_context);

// Initializes the LED driver, control modules and shared timer.
bool lc_init(void) {
    if (initialized) return false;

    if (!leds_pwm_init()) return false;

    hsv_init();
    yellow_init();

    if (app_timer_create(
            &control_timer,
            APP_TIMER_MODE_REPEATED,
            timer_handler) != NRF_SUCCESS)
        return false;

    initialized = true;

    if (app_timer_start(
            control_timer,
            APP_TIMER_TICKS(CONTROL_TIMER_INTERVAL_MS),
            NULL) != NRF_SUCCESS) {
        initialized = false;
        return false;
    }

    return true;
}

// Sets the RGB LED color through the PWM driver.
void lc_set_hsv(
    uint16_t hue,
    uint8_t saturation,
    uint8_t value) {
    leds_pwm_set_hsv(hue, saturation, value);
}

// Reads the current RGB LED color from the PWM driver.
void lc_get_hsv(
    uint16_t *hue,
    uint8_t *saturation,
    uint8_t *value) {
    leds_pwm_get_hsv(hue, saturation, value);
}

// Starts or resumes the selected HSV parameter change.
bool lc_start_hsv_change(lc_hsv_parameter_t parameter) {
    if (!initialized) return false;

    return hsv_start(parameter);
}

// Pauses the active HSV parameter change.
void lc_stop_hsv_change(void) {
    hsv_stop();
}

// Changes the yellow LED indication pattern.
bool lc_set_yellow_pattern(lc_yellow_pattern_t pattern) {
    if (!initialized) return false;

    return yellow_set_pattern(pattern);
}

// Advances both control modules using the same timer timestamp.
static void timer_handler(void *p_context) {
    uint32_t now = app_timer_cnt_get();

    (void)p_context;

    hsv_process(now);
    yellow_process(now);
}
