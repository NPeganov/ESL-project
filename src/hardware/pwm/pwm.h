#ifndef _PWM_H_
#define _PWM_H_

#include <stdbool.h>
#include <stdint.h>

// The nRF PWM peripheral provides four output channels.
#define PWM_CHANNEL_COUNT 4

// Identifies one of the four PWM output channels.
typedef enum {
    PWM_CHANNEL_0,
    PWM_CHANNEL_1,
    PWM_CHANNEL_2,
    PWM_CHANNEL_3
} pwm_channel_t;

// Defines which GPIO level is considered active for a channel.
typedef enum {
    PWM_POLARITY_ACTIVE_HIGH,
    PWM_POLARITY_ACTIVE_LOW
} pwm_polarity_t;

// GPIO pin and active-level configuration for one PWM channel.
typedef struct {
    uint8_t pin;
    pwm_polarity_t polarity;
} pwm_channel_config_t;

// Configuration shared by all channels of the PWM peripheral.
typedef struct {
    // Pin and polarity settings, indexed by pwm_channel_t.
    pwm_channel_config_t channels[PWM_CHANNEL_COUNT];
} pwm_config_t;

// Initialize the PWM peripheral. All channels start with zero duty.
// Call once before using pwm_set().
// Return false if the nrfx driver cannot be started.
// The configuration pointer, pins, and polarity values must be valid.
bool pwm_init(pwm_config_t const *config);

// Set a channel duty cycle from 0 (inactive) to 255 (active).
// The duty value has the same meaning for active-high and active-low channels.
void pwm_set(pwm_channel_t channel, uint8_t duty);

#endif
