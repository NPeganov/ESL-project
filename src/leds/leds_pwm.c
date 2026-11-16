#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nrf_assert.h"
#include "pca10059.h"

#include "leds_pwm.h"
#include "hardware/pwm/pwm.h"
#include "internal/color.h"

static bool m_initialized;
static color_rgb_t m_rgb;
static color_hsv_t m_hsv;
static uint8_t m_yellow_brightness;

static void rgb_apply(color_rgb_t rgb) {
    pwm_set(PWM_CHANNEL_1, rgb.red);
    pwm_set(PWM_CHANNEL_2, rgb.green);
    pwm_set(PWM_CHANNEL_3, rgb.blue);
}

bool leds_pwm_init(void) {
    ASSERT(!m_initialized);

    pwm_config_t config = {
        .frequency_hz = 1000,
        .channels = {
            { .pin = LED_1, .polarity = PWM_POLARITY_ACTIVE_LOW },
            { .pin = LED_2, .polarity = PWM_POLARITY_ACTIVE_LOW },
            { .pin = LED_3, .polarity = PWM_POLARITY_ACTIVE_LOW },
            { .pin = LED_4, .polarity = PWM_POLARITY_ACTIVE_LOW }
        }
    };

    if (!pwm_init(&config))
        return false;

    m_initialized = true;
    m_rgb = (color_rgb_t){ 0, 0, 0 };
    m_hsv = (color_hsv_t){ 0, 0, 0 };
    m_yellow_brightness = 0;

    return true;
}

void leds_pwm_set_rgb(uint8_t red, uint8_t green, uint8_t blue) {
    ASSERT(m_initialized);

    m_rgb.red = red;
    m_rgb.green = green;
    m_rgb.blue = blue;
    m_hsv = color_rgb_to_hsv(m_rgb);

    rgb_apply(m_rgb);
}

void leds_pwm_set_hsv(uint16_t hue, uint8_t saturation, uint8_t value) {
    ASSERT(m_initialized);
    ASSERT(hue < 360);
    ASSERT(saturation <= 100);
    ASSERT(value <= 100);

    m_hsv.hue = hue;
    m_hsv.saturation = saturation;
    m_hsv.value = value;
    m_rgb = color_hsv_to_rgb(m_hsv);

    rgb_apply(m_rgb);
}

void leds_pwm_get_rgb(uint8_t *red, uint8_t *green, uint8_t *blue) {
    ASSERT(m_initialized);
    ASSERT(red != NULL);
    ASSERT(green != NULL);
    ASSERT(blue != NULL);

    *red = m_rgb.red;
    *green = m_rgb.green;
    *blue = m_rgb.blue;
}

void leds_pwm_get_hsv(uint16_t *hue, uint8_t *saturation, uint8_t *value) {
    ASSERT(m_initialized);
    ASSERT(hue != NULL);
    ASSERT(saturation != NULL);
    ASSERT(value != NULL);

    *hue = m_hsv.hue;
    *saturation = m_hsv.saturation;
    *value = m_hsv.value;
}

void leds_pwm_set_yellow_brightness(uint8_t brightness) {
    ASSERT(m_initialized);
    ASSERT(brightness <= 100);

    m_yellow_brightness = brightness;
    pwm_set(PWM_CHANNEL_0, color_percent_to_value(brightness));
}

uint8_t leds_pwm_get_yellow_brightness(void) {
    ASSERT(m_initialized);

    return m_yellow_brightness;
}
