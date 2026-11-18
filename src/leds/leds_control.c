#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "leds_control.h"
#include "leds_pwm.h"
#include "leds_seq.h"

// Full Hue rotation and full S/V sweep durations.
#define HUE_CYCLE_DURATION_MS 10000
#define HSV_VALUE_SWEEP_DURATION_MS 5000

// Yellow indicator on/off durations.
#define YELLOW_SLOW_BLINK_MS 1000
#define YELLOW_FAST_BLINK_MS 150

static seq_step_t const m_hue_steps[] = {
    { 0, 360, HUE_CYCLE_DURATION_MS }
};

static seq_step_t const m_hsv_value_steps[] = {
    { 0, 100, HSV_VALUE_SWEEP_DURATION_MS }
};

static seq_step_t const m_yellow_off_steps[] = {
    { 0, 0, 0 }
};

static seq_step_t const m_yellow_on_steps[] = {
    { 0, 100, 0 }
};

static seq_step_t const m_yellow_slow_steps[] = {
    { 0, 100, 0 },
    { 100, 100, YELLOW_SLOW_BLINK_MS },
    { 100, 0, 0 },
    { 0, 0, YELLOW_SLOW_BLINK_MS }
};

static seq_step_t const m_yellow_fast_steps[] = {
    { 0, 100, 0 },
    { 100, 100, YELLOW_FAST_BLINK_MS },
    { 100, 0, 0 },
    { 0, 0, YELLOW_FAST_BLINK_MS }
};

static seq_config_t m_hue_config;
static seq_config_t m_hsv_value_config;
static seq_config_t m_yellow_off_config;
static seq_config_t m_yellow_on_config;
static seq_config_t m_yellow_slow_config;
static seq_config_t m_yellow_fast_config;

static bool m_initialized;
static bool m_active_parameter_valid;
static leds_control_hsv_parameter_t m_active_parameter;

static leds_control_hsv_parameter_t m_hue_context = LEDS_CONTROL_HUE;
static leds_control_hsv_parameter_t m_saturation_context = LEDS_CONTROL_SATURATION;
static leds_control_hsv_parameter_t m_value_context = LEDS_CONTROL_VALUE;

static void rgb_value_callback(uint16_t value, void *context);
static void yellow_callback(uint16_t value, void *context);

bool leds_control_init(void) {
    if (m_initialized) return false;

    if (!leds_pwm_init())
        return false;

    if (!seq_init())
        return false;

    m_hue_config.steps = m_hue_steps;
    m_hue_config.step_count = sizeof(m_hue_steps) / sizeof(m_hue_steps[0]);
    m_hue_config.repeat = true;
    m_hue_config.scale = SEQ_SCALE_CYCLIC;

    m_hsv_value_config.steps = m_hsv_value_steps;
    m_hsv_value_config.step_count = sizeof(m_hsv_value_steps) / sizeof(m_hsv_value_steps[0]);
    m_hsv_value_config.repeat = true;
    m_hsv_value_config.scale = SEQ_SCALE_BOUNCE;

    m_yellow_off_config.steps = m_yellow_off_steps;
    m_yellow_off_config.step_count = sizeof(m_yellow_off_steps) / sizeof(m_yellow_off_steps[0]);
    m_yellow_off_config.repeat = false;
    m_yellow_off_config.scale = SEQ_SCALE_LINEAR;

    m_yellow_on_config.steps = m_yellow_on_steps;
    m_yellow_on_config.step_count = sizeof(m_yellow_on_steps) / sizeof(m_yellow_on_steps[0]);
    m_yellow_on_config.repeat = false;
    m_yellow_on_config.scale = SEQ_SCALE_LINEAR;

    m_yellow_slow_config.steps = m_yellow_slow_steps;
    m_yellow_slow_config.step_count = sizeof(m_yellow_slow_steps) / sizeof(m_yellow_slow_steps[0]);
    m_yellow_slow_config.repeat = true;
    m_yellow_slow_config.scale = SEQ_SCALE_LINEAR;

    m_yellow_fast_config.steps = m_yellow_fast_steps;
    m_yellow_fast_config.step_count = sizeof(m_yellow_fast_steps) / sizeof(m_yellow_fast_steps[0]);
    m_yellow_fast_config.repeat = true;
    m_yellow_fast_config.scale = SEQ_SCALE_LINEAR;

    m_active_parameter_valid = false;
    m_initialized = true;

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
    seq_config_t const *config;
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;
    uint16_t current_value;
    void *context;

    if (!m_initialized) return false;

    if (parameter != LEDS_CONTROL_HUE &&
        parameter != LEDS_CONTROL_SATURATION &&
        parameter != LEDS_CONTROL_VALUE)
        return false;

    if (m_active_parameter_valid && m_active_parameter == parameter) {
        seq_state_t state = seq_get_state(RGB_LED);

        if (state == SEQ_STATE_RUNNING) return true;

        if (state == SEQ_STATE_PAUSED) {
            seq_resume(RGB_LED);
            return true;
        }
    }

    leds_pwm_get_hsv(&hue, &saturation, &value);

    switch (parameter) {
        case LEDS_CONTROL_HUE:
            config = &m_hue_config;
            current_value = hue;
            context = &m_hue_context;
            break;

        case LEDS_CONTROL_SATURATION:
            config = &m_hsv_value_config;
            current_value = saturation;
            context = &m_saturation_context;
            break;

        case LEDS_CONTROL_VALUE:
            config = &m_hsv_value_config;
            current_value = value;
            context = &m_value_context;
            break;

        default:
            return false;
    }

    if (!seq_start(
            RGB_LED,
            config,
            SEQ_START_CURRENT,
            current_value,
            rgb_value_callback,
            context))
        return false;

    m_active_parameter = parameter;
    m_active_parameter_valid = true;

    return true;
}

void leds_control_stop_hsv_change(void) {
    if (m_initialized)
        seq_pause(RGB_LED);
}

bool leds_control_set_yellow_pattern(
    leds_control_yellow_pattern_t pattern) {
    seq_config_t const *config;

    if (!m_initialized) return false;

    switch (pattern) {
        case LEDS_CONTROL_YELLOW_OFF:
            config = &m_yellow_off_config;
            break;

        case LEDS_CONTROL_YELLOW_SLOW_BLINK:
            config = &m_yellow_slow_config;
            break;

        case LEDS_CONTROL_YELLOW_FAST_BLINK:
            config = &m_yellow_fast_config;
            break;

        case LEDS_CONTROL_YELLOW_ON:
            config = &m_yellow_on_config;
            break;

        default:
            return false;
    }

    return seq_start(
        YELLOW_LED,
        config,
        SEQ_START_FROM_STEP,
        config->steps[0].from,
        yellow_callback,
        NULL);
}

static void rgb_value_callback(uint16_t value, void *context) {
    leds_control_hsv_parameter_t const *parameter = context;
    uint16_t hue;
    uint8_t saturation;
    uint8_t brightness;

    leds_pwm_get_hsv(&hue, &saturation, &brightness);

    switch (*parameter) {
        case LEDS_CONTROL_HUE:
            hue = value;
            break;

        case LEDS_CONTROL_SATURATION:
            saturation = (uint8_t)value;
            break;

        case LEDS_CONTROL_VALUE:
            brightness = (uint8_t)value;
            break;

        default:
            return;
    }

    leds_pwm_set_hsv(hue, saturation, brightness);
}

static void yellow_callback(uint16_t value, void *context) {
    (void)context;

    leds_pwm_set_yellow_brightness((uint8_t)value);
}
