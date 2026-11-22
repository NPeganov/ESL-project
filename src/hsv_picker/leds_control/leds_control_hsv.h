#ifndef _LEDS_CONTROL_HSV_H_
#define _LEDS_CONTROL_HSV_H_

#include <stdbool.h>
#include <stdint.h>

#include "leds_control.h"

// Resets the HSV change controller to its idle state.
void hsv_init(void);

// Starts a parameter change or resumes the same paused parameter.
// Returns false if the requested parameter is invalid.
bool hsv_start(lc_hsv_parameter_t parameter);

// Pauses the active change while preserving its elapsed progress.
void hsv_stop(void);

// Advances the active change using the supplied app_timer counter value.
void hsv_process(uint32_t now);    // Current app_timer counter value.

#endif
