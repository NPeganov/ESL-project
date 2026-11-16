#include <stdint.h>

#include "nrfx_systick.h"

#include "btn/btn.h"
#include "leds/leds_pwm.h"
#include "seq/seq.h"

// Yellow LED: a 2-second brightness cycle.
static seq_step_t const m_yellow_steps[] = {
    { 0, 100, 500 },
    { 100, 100, 500 },
    { 100, 0, 500 },
    { 0, 0, 500 }
};

// RGB LED: the same cycle shifted by 500 ms.
static seq_step_t const m_rgb_steps[] = {
    { 100, 100, 500 },
    { 100, 0, 500 },
    { 0, 0, 500 },
    { 0, 100, 500 }
};

static seq_config_t const m_yellow_config = {
    .steps = m_yellow_steps,
    .step_count = sizeof(m_yellow_steps) / sizeof(m_yellow_steps[0]),
    .repeat = true
};

static seq_config_t const m_rgb_config = {
    .steps = m_rgb_steps,
    .step_count = sizeof(m_rgb_steps) / sizeof(m_rgb_steps[0]),
    .repeat = true
};

static void yellow_callback(uint16_t value, void *context);
static void rgb_value_callback(uint16_t value, void *context);

int main(void) {
    nrfx_systick_init();

    // btn_init() initializes LFCLK and the shared app_timer framework.
    btn_init();

    if (!leds_pwm_init())
        while (1) nrfx_systick_delay_ms(1000);

    // Keep the RGB hue and saturation fixed while the sequence changes Value.
    leds_pwm_set_hsv(0, 100, 100);

    if (!seq_init())
        while (1) nrfx_systick_delay_ms(1000);

    if (!seq_start(YELLOW_LED, &m_yellow_config, yellow_callback, NULL))
        while (1) nrfx_systick_delay_ms(1000);

    if (!seq_start(RGB_LED, &m_rgb_config, rgb_value_callback, NULL))
        while (1) nrfx_systick_delay_ms(1000);

    // Pause and resume RGB while the yellow sequence keeps running.
    nrfx_systick_delay_ms(3000);
    seq_pause(RGB_LED);
    nrfx_systick_delay_ms(1000);
    seq_resume(RGB_LED);

    // Pause and resume yellow while the RGB sequence keeps running.
    nrfx_systick_delay_ms(3000);
    seq_pause(YELLOW_LED);
    nrfx_systick_delay_ms(1000);
    seq_resume(YELLOW_LED);

    while (1)
        nrfx_systick_delay_ms(1000);
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
