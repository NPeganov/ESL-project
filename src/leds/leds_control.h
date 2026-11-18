#ifndef _LEDS_CONTROL_H_
#define _LEDS_CONTROL_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LEDS_CONTROL_HUE,
    LEDS_CONTROL_SATURATION,
    LEDS_CONTROL_VALUE
} leds_control_hsv_parameter_t;

typedef enum {
    LEDS_CONTROL_YELLOW_OFF,
    LEDS_CONTROL_YELLOW_SLOW_BLINK,
    LEDS_CONTROL_YELLOW_FAST_BLINK,
    LEDS_CONTROL_YELLOW_ON
} leds_control_yellow_pattern_t;

bool leds_control_init(void);

void leds_control_set_hsv(
    uint16_t hue,
    uint8_t saturation,
    uint8_t value);

void leds_control_get_hsv(
    uint16_t *hue,
    uint8_t *saturation,
    uint8_t *value);

bool leds_control_start_hsv_change(
    leds_control_hsv_parameter_t parameter);

void leds_control_stop_hsv_change(void);

bool leds_control_set_yellow_pattern(
    leds_control_yellow_pattern_t pattern);

#endif
