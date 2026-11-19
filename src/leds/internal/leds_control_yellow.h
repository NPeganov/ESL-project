#ifndef _LEDS_CONTROL_YELLOW_H_
#define _LEDS_CONTROL_YELLOW_H_

#include <stdbool.h>
#include <stdint.h>

#include "../leds_control.h"

// Sets the initial yellow LED state to off.
void yellow_init(void);

// Applies a yellow LED pattern. Returns false for an unsupported pattern.
bool yellow_set_pattern(lc_yellow_pattern_t pattern);

// Advances the blinking pattern using the supplied timer timestamp.
void yellow_process(uint32_t now);    // Current app_timer counter value.

#endif
