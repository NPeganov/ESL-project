#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nrf_assert.h"
#include "pca10059.h"

#include "leds_pwm.h"
#include "hardware/pwm/pwm.h"
#include "internal/color.h"

// Control the yellow and RGB LEDs through hardware PWM.

static bool m_initialized;             // True after successful initialization.
static color_rgb_t m_rgb;              // Current RGB color.
static color_hsv_t m_hsv;              // Current HSV color.
static uint8_t m_yellow_brightness;    // Current yellow LED brightness, percent.

// Apply the RGB channel values to the corresponding PWM channels.
static void rgb_apply(color_rgb_t rgb) {
    pwm_set(PWM_CHANNEL_1, rgb.red);
    pwm_set(PWM_CHANNEL_2, rgb.green);
    pwm_set(PWM_CHANNEL_3, rgb.blue);
}

// Initialize PWM for the four active-low LEDs.
bool leds_pwm_init(void) {
    ASSERT(!m_initialized);

    pwm_config_t config;
    config.frequency_hz = 1000;
    config.channels[0].pin = LED_1;
    config.channels[0].polarity = PWM_POLARITY_ACTIVE_LOW;
    config.channels[1].pin = LED_2;
    config.channels[1].polarity = PWM_POLARITY_ACTIVE_LOW;
    config.channels[2].pin = LED_3;
    config.channels[2].polarity = PWM_POLARITY_ACTIVE_LOW;
    config.channels[3].pin = LED_4;
    config.channels[3].polarity = PWM_POLARITY_ACTIVE_LOW;

    if (!pwm_init(&config)) return false;

    m_initialized = true;
    m_rgb = (color_rgb_t){ 0, 0, 0 };
    m_hsv = (color_hsv_t){ 0, 0, 0 };
    m_yellow_brightness = 0;

    return true;
}

// Set the RGB LED color and update its HSV representation.
void leds_pwm_set_rgb(uint8_t red, uint8_t green, uint8_t blue) {
    ASSERT(m_initialized);

    m_rgb.red = red;
    m_rgb.green = green;
    m_rgb.blue = blue;
    m_hsv = color_rgb_to_hsv(m_rgb);

    rgb_apply(m_rgb);
}

// Set the RGB LED color and update its RGB representation.
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

// Return the current RGB color.
void leds_pwm_get_rgb(uint8_t *red, uint8_t *green, uint8_t *blue) {
    ASSERT(m_initialized);
    ASSERT(red != NULL);
    ASSERT(green != NULL);
    ASSERT(blue != NULL);

    *red = m_rgb.red;
    *green = m_rgb.green;
    *blue = m_rgb.blue;
}

// Return the current HSV color.
void leds_pwm_get_hsv(uint16_t *hue, uint8_t *saturation, uint8_t *value) {
    ASSERT(m_initialized);
    ASSERT(hue != NULL);
    ASSERT(saturation != NULL);
    ASSERT(value != NULL);

    *hue = m_hsv.hue;
    *saturation = m_hsv.saturation;
    *value = m_hsv.value;
}

// Set the yellow LED brightness in percent.
void leds_pwm_set_yellow_brightness(uint8_t brightness) {
    ASSERT(m_initialized);
    ASSERT(brightness <= 100);

    m_yellow_brightness = brightness;
    pwm_set(PWM_CHANNEL_0, color_percent_to_value(brightness));
}

// Return the current yellow LED brightness in percent.
uint8_t leds_pwm_get_yellow_brightness(void) {
    ASSERT(m_initialized);

    return m_yellow_brightness;
}
