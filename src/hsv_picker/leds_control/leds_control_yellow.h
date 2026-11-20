#ifndef _LEDS_CONTROL_YELLOW_H_
#define _LEDS_CONTROL_YELLOW_H_

#include <stdbool.h>
#include <stdint.h>

#include "leds_control.h"

// Sets the initial yellow LED pattern to off.
void yellow_init(void);

// Applies a yellow LED pattern; returns false if the pattern is unsupported.
bool yellow_set_pattern(lc_yellow_pattern_t pattern);

// Updates the selected pattern using the supplied app_timer counter value.
void yellow_process(uint32_t now);    // Current app_timer counter value.

#endif
