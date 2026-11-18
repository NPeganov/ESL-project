#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "leds/leds_pwm.h"
#include "leds/leds_seq.h"

#include "main_behavior.h"

// Yellow LED: slow breathing with a four-second period.
static seq_step_t const m_yellow_steps[] = {
    { 0, 100, 1500 },
    { 100, 100, 500 },
    { 100, 0, 1500 },
    { 0, 0, 500 }
};

// Alternate yellow sequence: fast blinking with a half-second period.
static seq_step_t const m_yellow_alternate_steps[] = {
    { 0, 100, 80 },
    { 100, 100, 170 },
    { 100, 0, 80 },
    { 0, 0, 170 }
};

// RGB LED runs an independent six-second brightness cycle.
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

    if (!seq_start(
            YELLOW_LED,
            &m_yellow_config,
            SEQ_START_FROM_STEP,
            m_yellow_steps[0].from,
            yellow_callback,
            NULL))
        return false;

    if (!seq_start(
            RGB_LED,
            &m_rgb_config,
            SEQ_START_FROM_STEP,
            m_rgb_steps[0].from,
            rgb_value_callback,
            NULL))
        return false;

    return true;
}

bool main_behavior_toggle_yellow(void) {
    seq_config_t const *config;

    m_yellow_alternate = !m_yellow_alternate;
    config = m_yellow_alternate ? &m_yellow_alternate_config : &m_yellow_config;

    // Replace the current yellow sequence from its explicitly defined first step.
    return seq_start(
        YELLOW_LED,
        config,
        SEQ_START_FROM_STEP,
        config->steps[0].from,
        yellow_callback,
        NULL);
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
