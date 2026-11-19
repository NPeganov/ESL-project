#include <stdbool.h>
#include <stdint.h>

#include "app_timer.h"
#include "nrf_error.h"

#include "leds_control.h"
#include "leds_pwm.h"
#include "leds/internal/leds_control_hsv.h"
#include "leds/internal/leds_control_yellow.h"

#define CONTROL_TIMER_INTERVAL_MS 10

APP_TIMER_DEF(m_control_timer);

static bool m_initialized;

static void control_timer_handler(void *p_context);

bool leds_control_init(void) {
    if (m_initialized) return false;

    if (!leds_pwm_init()) return false;

    leds_control_hsv_init();
    leds_control_yellow_init();

    if (app_timer_create(
            &m_control_timer,
            APP_TIMER_MODE_REPEATED,
            control_timer_handler) != NRF_SUCCESS)
        return false;

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
    if (!m_initialized) return false;

    return leds_control_hsv_start(parameter);
}

void leds_control_stop_hsv_change(void) {
    leds_control_hsv_stop();
}

bool leds_control_set_yellow_pattern(
    leds_control_yellow_pattern_t pattern) {
    if (!m_initialized) return false;

    return leds_control_yellow_set_pattern(pattern);
}

static void control_timer_handler(void *p_context) {
    uint32_t now = app_timer_cnt_get();

    (void)p_context;

    leds_control_hsv_process(now);
    leds_control_yellow_process(now);
}
