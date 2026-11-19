#ifndef _LEDS_CONTROL_YELLOW_H_
#define _LEDS_CONTROL_YELLOW_H_

#include <stdbool.h>
#include <stdint.h>

#include "../leds_control.h"

void leds_control_yellow_init(void);

bool leds_control_yellow_set_pattern(
    leds_control_yellow_pattern_t pattern);

void leds_control_yellow_process(uint32_t now);

#endif
