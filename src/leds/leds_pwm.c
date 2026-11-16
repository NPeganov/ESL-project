#include <stdbool.h>
#include <stdint.h>

#include "nrf_assert.h"
#include "pca10059.h"

#include "leds_pwm.h"
#include "pwm/pwm.h"

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rgb_t;

typedef struct {
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;
} hsv_t;

static bool m_initialized;
static rgb_t m_rgb;
static hsv_t m_hsv;
static uint8_t m_yellow_brightness;

static uint8_t percent_to_duty(uint8_t percent) {
    return (uint8_t)((percent * 255 + 50) / 100);
}

static uint8_t scale_to_percent(uint8_t value) {
    return (uint8_t)((value * 100 + 127) / 255);
}

static rgb_t hsv_to_rgb(hsv_t hsv) {
    rgb_t rgb;
    uint8_t value = percent_to_duty(hsv.value);

    if (hsv.saturation == 0) {
        rgb.red = value;
        rgb.green = value;
        rgb.blue = value;
        return rgb;
    }

    uint32_t hue = (uint32_t)hsv.hue * 6;
    uint32_t sector = hue / 360;
    uint32_t fraction = hue % 360;

    uint8_t p = (uint8_t)((value * (100 - hsv.saturation) + 50) / 100);
    uint8_t q = (uint8_t)(
        (value * (100 - (hsv.saturation * fraction + 180) / 360) + 50) / 100);
    uint8_t t = (uint8_t)(
        (value * (100 - (hsv.saturation * (360 - fraction) + 180) / 360) + 50) / 100);

    switch (sector) {
        case 0:
            rgb.red = value;
            rgb.green = t;
            rgb.blue = p;
            break;
        case 1:
            rgb.red = q;
            rgb.green = value;
            rgb.blue = p;
            break;
        case 2:
            rgb.red = p;
            rgb.green = value;
            rgb.blue = t;
            break;
        case 3:
            rgb.red = p;
            rgb.green = q;
            rgb.blue = value;
            break;
        case 4:
            rgb.red = t;
            rgb.green = p;
            rgb.blue = value;
            break;
        default:
            rgb.red = value;
            rgb.green = p;
            rgb.blue = q;
            break;
    }

    return rgb;
}

static hsv_t rgb_to_hsv(rgb_t rgb) {
    hsv_t hsv;
    uint8_t max = rgb.red;
    uint8_t min = rgb.red;

    if (rgb.green > max)
        max = rgb.green;
    if (rgb.blue > max)
        max = rgb.blue;
    if (rgb.green < min)
        min = rgb.green;
    if (rgb.blue < min)
        min = rgb.blue;

    uint8_t delta = max - min;

    hsv.value = scale_to_percent(max);

    if (max == 0) {
        hsv.saturation = 0;
        hsv.hue = 0;
        return hsv;
    }

    hsv.saturation = (uint8_t)((delta * 100 + max / 2) / max);

    if (delta == 0) {
        hsv.hue = 0;
        return hsv;
    }

    if (max == rgb.red) {
        int32_t hue = 60 * ((int32_t)rgb.green - rgb.blue) / delta;
        if (hue < 0)
            hue += 360;
        hsv.hue = (uint16_t)hue;
    } else if (max == rgb.green) {
        hsv.hue = (uint16_t)(120 + 60 * ((int32_t)rgb.blue - rgb.red) / delta);
    } else {
        hsv.hue = (uint16_t)(240 + 60 * ((int32_t)rgb.red - rgb.green) / delta);
    }

    return hsv;
}

static void rgb_apply(rgb_t rgb) {
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
    m_rgb = (rgb_t){ 0, 0, 0 };
    m_hsv = (hsv_t){ 0, 0, 0 };
    m_yellow_brightness = 0;

    return true;
}

void leds_pwm_set_rgb(uint8_t red, uint8_t green, uint8_t blue) {
    ASSERT(m_initialized);

    m_rgb.red = red;
    m_rgb.green = green;
    m_rgb.blue = blue;
    m_hsv = rgb_to_hsv(m_rgb);

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
    m_rgb = hsv_to_rgb(m_hsv);

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
    pwm_set(PWM_CHANNEL_0, percent_to_duty(brightness));
}

uint8_t leds_pwm_get_yellow_brightness(void) {
    ASSERT(m_initialized);

    return m_yellow_brightness;
}
