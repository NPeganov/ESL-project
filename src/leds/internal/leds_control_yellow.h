#ifndef _LEDS_CONTROL_YELLOW_H_
#define _LEDS_CONTROL_YELLOW_H_

#include <stdbool.h>
#include <stdint.h>

#include "../leds_control.h"

void yellow_init(void);

bool yellow_set_pattern(lc_yellow_pattern_t pattern);

void yellow_process(uint32_t now);

#endif
