#ifndef _LEDS_CONTROL_HSV_H_
#define _LEDS_CONTROL_HSV_H_

#include <stdbool.h>
#include <stdint.h>

#include "../leds_control.h"

void leds_control_hsv_init(void);

bool leds_control_hsv_start(leds_control_hsv_parameter_t parameter);

void leds_control_hsv_stop(void);

void leds_control_hsv_process(uint32_t now);

#endif
