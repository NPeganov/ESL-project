#ifndef _LEDS_CONTROL_HSV_H_
#define _LEDS_CONTROL_HSV_H_

#include <stdbool.h>
#include <stdint.h>

#include "../leds_control.h"

void hsv_init(void);

bool hsv_start(lc_hsv_parameter_t parameter);

void hsv_stop(void);

void hsv_process(uint32_t now);

#endif
