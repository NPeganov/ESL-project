#ifndef _LEDS_CONTROL_HSV_H_
#define _LEDS_CONTROL_HSV_H_

#include <stdbool.h>
#include <stdint.h>

#include "../leds_control.h"

// Resets the HSV change state.
void hsv_init(void);

// Starts changing a parameter, or resumes the same paused parameter.
bool hsv_start(lc_hsv_parameter_t parameter);

// Pauses the active change while preserving its phase.
void hsv_stop(void);

// Updates the selected HSV parameter using the supplied timer timestamp.
void hsv_process(uint32_t now);    // Current app_timer counter value.

#endif
