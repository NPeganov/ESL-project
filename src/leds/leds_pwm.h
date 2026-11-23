#ifndef _LEDS_PWM_H_
#define _LEDS_PWM_H_

#include <stdbool.h>
#include <stdint.h>

// Initialize the PWM channels used by the yellow and RGB LEDs.
bool leds_pwm_init(void);

// Set the RGB LED color using 8-bit RGB components.
void leds_pwm_set_rgb(uint8_t red, uint8_t green, uint8_t blue);

// Set the RGB LED color using hue 0..359 degrees,
// saturation 0..100 percent, and value 0..100 percent.
void leds_pwm_set_hsv(uint16_t hue, uint8_t saturation, uint8_t value);

// Return the current RGB representation of the RGB LED color.
void leds_pwm_get_rgb(uint8_t *red, uint8_t *green, uint8_t *blue);

// Return the current HSV representation of the RGB LED color.
void leds_pwm_get_hsv(uint16_t *hue, uint8_t *saturation, uint8_t *value);

// Set the yellow LED brightness from 0 (off) to 100 percent.
void leds_pwm_set_yellow_brightness(uint8_t brightness);

// Return the current yellow LED brightness in percent.
uint8_t leds_pwm_get_yellow_brightness(void);

#endif
