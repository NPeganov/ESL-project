#include <stdbool.h>
#include <stdint.h>

#include "nrfx_systick.h"

#include "btn/btn.h"
#include "leds/leds_pwm.h"
#include "seq/seq.h"

// Yellow LED: a 6-second brightness cycle.
static seq_step_t const m_yellow_steps[] = {
    { 0, 100, 2000 },
    { 100, 100, 1000 },
    { 100, 0, 2000 },
    { 0, 0, 1000 }
};

// RGB LED: the same cycle shifted by 3 seconds.
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

static seq_config_t const m_rgb_config = {
    .steps = m_rgb_steps,
    .step_count = sizeof(m_rgb_steps) / sizeof(m_rgb_steps[0]),
    .repeat = true
};

static void yellow_callback(uint16_t value, void *context);
static void rgb_value_callback(uint16_t value, void *context);
static void executor_toggle(seq_led_id id);

int main(void) {
    bool single_press_pending = false;
    uint16_t single_press_timeout_ms = 0;

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

    while (1) {
        btn_event_t event;

        // A double click also produces SINGLE_PRESS events. Delay the single
        // action until the double-press window has expired.
        while ((event = btn_event_take()) != BTN_EVENT_NONE) {
            if (event == BTN_EVENT_SINGLE_PRESS) {
                single_press_pending = true;
                // Must exceed BTN_DOUBLE_PRESS_TIME_MS in btn.c.
                single_press_timeout_ms = 1100;
            } else if (event == BTN_EVENT_DOUBLE_PRESS) {
                single_press_pending = false;
                single_press_timeout_ms = 0;
                executor_toggle(RGB_LED);
            }
        }

        if (single_press_pending) {
            if (single_press_timeout_ms <= 10) {
                single_press_pending = false;
                executor_toggle(YELLOW_LED);
            } else
                single_press_timeout_ms -= 10;
        }

        nrfx_systick_delay_ms(10);
    }
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

static void executor_toggle(seq_led_id id) {
    switch (seq_get_state(id)) {
        case SEQ_STATE_RUNNING:
            seq_pause(id);
            break;

        case SEQ_STATE_PAUSED:
            seq_resume(id);
            break;

        default:
            break;
    }
}
