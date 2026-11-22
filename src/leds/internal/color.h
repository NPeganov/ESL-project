#ifndef _COLOR_H_
#define _COLOR_H_

#include <stdint.h>

typedef struct {
    uint8_t red;   // Red channel, 0-255
    uint8_t green; // Green channel, 0-255
    uint8_t blue;  // Blue channel, 0-255
} color_rgb_t;

typedef struct {
    uint16_t hue;        // Hue angle, 0-359 degrees
    uint8_t saturation;  // Saturation, 0-100%
    uint8_t value;       // Value (brightness), 0-100%
} color_hsv_t;

// Convert a percentage (0-100) to an 8-bit channel value (0-255).
uint8_t color_percent_to_value(uint8_t percent);

// Convert between HSV and RGB color representations.
color_rgb_t color_hsv_to_rgb(color_hsv_t hsv);
color_hsv_t color_rgb_to_hsv(color_rgb_t rgb);

#endif
