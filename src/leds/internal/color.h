#ifndef _COLOR_H_
#define _COLOR_H_

#include <stdint.h>

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} color_rgb_t;

typedef struct {
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;
} color_hsv_t;

uint8_t color_percent_to_value(uint8_t percent);
color_rgb_t color_hsv_to_rgb(color_hsv_t hsv);
color_hsv_t color_rgb_to_hsv(color_rgb_t rgb);

#endif
