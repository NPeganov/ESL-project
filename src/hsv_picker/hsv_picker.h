#ifndef _HSV_PICKER_H_
#define _HSV_PICKER_H_

#include <stdbool.h>

// Initialize the HSV picker and all services it depends on.
// Must be called successfully before hsv_picker_start().
bool hsv_picker_init(void);

// Enable button interaction and start the picker in NO_INPUT mode.
// The yellow LED is turned off until the user selects an input mode.
void hsv_picker_start(void);

// Process deferred operations requested by button events.
void hsv_picker_process(void);

#endif
