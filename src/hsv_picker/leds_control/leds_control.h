#ifndef _LEDS_CONTROL_H_
#define _LEDS_CONTROL_H_

#include <stdbool.h>
#include <stdint.h>

// HSV component whose value changes while the button is held.
typedef enum {
    LC_HUE,
    LC_SATURATION,
    LC_VALUE
} lc_change_parameter_t;

// Yellow LED indication patterns used to show the current picker mode.
typedef enum {
    LC_YELLOW_OFF,
    LC_YELLOW_SLOW_BLINK,
    LC_YELLOW_FAST_BLINK,
    LC_YELLOW_ON
} lc_yellow_pattern_t;

// Initializes the PWM driver, both control modules and the periodic timer.
// Must be called before starting HSV changes or setting an indication pattern.
// Returns false if initialization fails or has already been performed.
bool lc_init(void);

// Sets the RGB LED color. Hue is in degrees (0..359); saturation and value are 0..100.
void lc_set_hsv(
    uint16_t hue,           // Hue in degrees.
    uint8_t saturation,     // Saturation, 0..100.
    uint8_t value);         // Value, 0..100.

// Gets the current RGB LED color. All output pointers must be valid.
void lc_get_hsv(
    uint16_t *hue,          // Receives hue in degrees.
    uint8_t *saturation,    // Receives saturation, 0..100.
    uint8_t *value);        // Receives value, 0..100.

// Starts changing the selected component, or resumes it if previously paused.
void lc_start_hsv_change(lc_change_parameter_t parameter);

// Pauses the active HSV change. Starting the same component resumes its progress.
void lc_stop_hsv_change(void);

// Sets the yellow LED indication pattern.
// Returns false if the module is not initialized or the pattern is invalid.
bool lc_set_yellow_pattern(lc_yellow_pattern_t pattern);

#endif
