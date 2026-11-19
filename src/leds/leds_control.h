#ifndef _LEDS_CONTROL_H_
#define _LEDS_CONTROL_H_

#include <stdbool.h>
#include <stdint.h>

// HSV parameter controlled while the button is held.
typedef enum {
    LC_HUE,
    LC_SATURATION,
    LC_VALUE
} lc_hsv_parameter_t;

// Yellow LED indication pattern.
typedef enum {
    LC_YELLOW_OFF,
    LC_YELLOW_SLOW_BLINK,
    LC_YELLOW_FAST_BLINK,
    LC_YELLOW_ON
} lc_yellow_pattern_t;

// Initializes PWM, both control modules and the periodic control timer.
// Returns false if initialization fails or has already been performed.
bool lc_init(void);

// Sets the RGB LED color. Hue is in degrees (0..359); saturation and value are 0..100.
void lc_set_hsv(
    uint16_t hue,           // Hue in degrees.
    uint8_t saturation,     // Saturation, 0..100.
    uint8_t value);         // Value, 0..100.

// Gets the current RGB LED color. All output pointers must be valid.
void lc_get_hsv(
    uint16_t *hue,          // Receives Hue in degrees.
    uint8_t *saturation,    // Receives saturation, 0..100.
    uint8_t *value);        // Receives value, 0..100.

// Starts changing the selected HSV parameter, or resumes it if paused.
// Returns false if the module is not initialized or the parameter is invalid.
bool lc_start_hsv_change(lc_hsv_parameter_t parameter);

// Pauses the current HSV change. Starting the same parameter resumes its phase.
void lc_stop_hsv_change(void);

// Sets the yellow LED indication pattern.
// Returns false if the module is not initialized or the pattern is invalid.
bool lc_set_yellow_pattern(lc_yellow_pattern_t pattern);

#endif
