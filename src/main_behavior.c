#include <stdbool.h>
#include <stdint.h>

#include "leds/leds_pwm.h"
#include "leds/leds_seq.h"

#include "main_behavior.h"

// Yellow LED: a 6-second brightness cycle with slow ramps.
static seq_step_t const m_yellow_steps[] = {
    { 0, 100, 2000 },
    { 100, 100, 1000 },
    { 100, 0, 2000 },
    { 0, 0, 1000 }
};

// Alternate yellow sequence: fast ramps and long holds.
static seq_step_t const m_yellow_alternate_steps[] = {
    { 0, 100, 500 },
    { 100, 100, 2000 },
    { 100, 0, 500 },
    { 0, 0, 2000 }
};

// RGB LED keeps its independent 6-second brightness cycle.
static seq_step_t const m_rgb_steps[] = {
    { 100, 100, 1000 },
    { 100, 0, 2000 },
    { 0, 0, 1000 },
    { 0, 100, 2000 }
};

static seq_config_t const m_yellow_config = {
    .steps = m_yellow_steps,
    .step_count = sizeof(m_yellow_steps) / sizeof(m_yellow_steps[0]),
    .repeat = true
};

static seq_config_t const m_yellow_alternate_config = {
    .steps = m_yellow_alternate_steps,
    .step_count = sizeof(m_yellow_alternate_steps) / sizeof(m_yellow_alternate_steps[0]),
    .repeat = true
};

static seq_config_t const m_rgb_config = {
    .steps = m_rgb_steps,
    .step_count = sizeof(m_rgb_steps) / sizeof(m_rgb_steps[0]),
    .repeat = true
};

static bool m_yellow_alternate = false;

static void yellow_callback(uint16_t value, void *context);
static void rgb_value_callback(uint16_t value, void *context);

bool main_behavior_init(void) {
    if (!leds_pwm_init())
        return false;

    // Keep the RGB hue and saturation fixed while the sequence changes Value.
    leds_pwm_set_hsv(0, 100, 100);

    if (!seq_init())
        return false;

    if (!seq_start(YELLOW_LED, &m_yellow_config, yellow_callback, NULL))
        return false;

    if (!seq_start(RGB_LED, &m_rgb_config, rgb_value_callback, NULL))
        return false;

    return true;
}

bool main_behavior_toggle_yellow(void) {
    seq_config_t const *config;

    m_yellow_alternate = !m_yellow_alternate;
    config = m_yellow_alternate ? &m_yellow_alternate_config : &m_yellow_config;

    // seq_start() replaces the current sequence and starts the selected route
    // from its explicitly defined first step.
    return seq_start(YELLOW_LED, config, yellow_callback, NULL);
}

static void yellow_callback(uint16_t value, void *context) {
    (void)context;

    leds_pwm_set_yellow_brightness((uint8_t)value);
}

static void rgb_value_callback(uint16_t value, void *context) {
    uint16_t hue;
    uint8_t saturation;
    uint8_t brightness;

    (void)context;

    leds_pwm_get_hsv(&hue, &saturation, &brightness);
    leds_pwm_set_hsv(hue, saturation, (uint8_t)value);
}
