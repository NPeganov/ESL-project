#ifndef _LEDS_CONTROL_H_
#define _LEDS_CONTROL_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LC_HUE,
    LC_SATURATION,
    LC_VALUE
} lc_hsv_parameter_t;

typedef enum {
    LC_YELLOW_OFF,
    LC_YELLOW_SLOW_BLINK,
    LC_YELLOW_FAST_BLINK,
    LC_YELLOW_ON
} lc_yellow_pattern_t;

bool lc_init(void);

void lc_set_hsv(
    uint16_t hue,
    uint8_t saturation,
    uint8_t value);

void lc_get_hsv(
    uint16_t *hue,
    uint8_t *saturation,
    uint8_t *value);

bool lc_start_hsv_change(lc_hsv_parameter_t parameter);

void lc_stop_hsv_change(void);

bool lc_set_yellow_pattern(lc_yellow_pattern_t pattern);

#endif
