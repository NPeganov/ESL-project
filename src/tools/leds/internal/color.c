#include <stdint.h>

#include "color.h"

// Convert between HSV percentages/degrees and 8-bit RGB channels.
uint8_t color_percent_to_value(uint8_t percent) {
    return (uint8_t)((percent * 255 + 50) / 100);
}

// Convert an 8-bit channel value to a percentage, rounded to nearest.
static uint8_t scale_to_percent(uint8_t value) {
    return (uint8_t)((value * 100 + 127) / 255);
}

// Convert HSV (H: 0-359 degrees, S/V: 0-100%) to 8-bit RGB.
color_rgb_t color_hsv_to_rgb(color_hsv_t hsv) {
    color_rgb_t rgb;
    uint8_t value = color_percent_to_value(hsv.value);

    // Zero saturation produces a grayscale color.
    if (hsv.saturation == 0) {
        rgb.red = value;
        rgb.green = value;
        rgb.blue = value;
        return rgb;
    }

    // Split the hue wheel into six sectors and find the position within one.
    uint32_t hue = (uint32_t)hsv.hue * 6;
    uint32_t sector = hue / 360;
    uint32_t fraction = hue % 360;

    // Calculate the three channel levels used by the HSV-to-RGB sectors.
    uint8_t p = (uint8_t)((value * (100 - hsv.saturation) + 50) / 100);
    uint8_t q = (uint8_t)(
        (value * (100 - (hsv.saturation * fraction + 180) / 360) + 50) / 100);
    uint8_t t = (uint8_t)(
        (value * (100 - (hsv.saturation * (360 - fraction) + 180) / 360) + 50) / 100);

    switch (sector) {
        case 0: rgb.red = value; rgb.green = t; rgb.blue = p; break;
        case 1: rgb.red = q; rgb.green = value; rgb.blue = p; break;
        case 2: rgb.red = p; rgb.green = value; rgb.blue = t; break;
        case 3: rgb.red = p; rgb.green = q; rgb.blue = value; break;
        case 4: rgb.red = t; rgb.green = p; rgb.blue = value; break;
        default: rgb.red = value; rgb.green = p; rgb.blue = q; break;
    }

    return rgb;
}

// Convert 8-bit RGB channels to HSV (H: degrees, S/V: percentages).
color_hsv_t color_rgb_to_hsv(color_rgb_t rgb) {
    color_hsv_t hsv;
    uint8_t max = rgb.red;
    uint8_t min = rgb.red;

    if (rgb.green > max) max = rgb.green;
    if (rgb.blue > max) max = rgb.blue;
    if (rgb.green < min) min = rgb.green;
    if (rgb.blue < min) min = rgb.blue;

    uint8_t delta = max - min;

    hsv.value = scale_to_percent(max);

    // Black has no saturation or defined hue.
    if (max == 0) {
        hsv.saturation = 0;
        hsv.hue = 0;
        return hsv;
    }

    hsv.saturation = (uint8_t)((delta * 100 + max / 2) / max);

    // A grayscale color has no defined hue.
    if (delta == 0) {
        hsv.hue = 0;
        return hsv;
    }

    // Calculate hue from the channel with the maximum value.
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
