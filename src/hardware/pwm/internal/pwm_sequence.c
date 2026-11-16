#include <stdbool.h>
#include <stdint.h>

#include "nrfx_pwm.h"

#include "pwm_internal.h"

// Bit 15 selects falling-edge polarity for an active-low PWM output.
#define PWM_VALUE_POLARITY_MASK 0x8000

// Convert the duty cycle to a PWM compare value and apply its polarity.
static uint16_t pwm_channel_value(
    uint8_t duty,
    uint16_t top_value,
    bool active_low) {

    // Convert the 0..255 duty value to active PWM ticks, rounded to nearest.
    uint32_t active_ticks =
        ((uint32_t)duty * top_value + UINT8_MAX / 2) / UINT8_MAX;

    // The compare value represents the inactive part of the PWM period.
    uint16_t value = top_value - active_ticks;

    if (active_low)
        value |= PWM_VALUE_POLARITY_MASK;

    return value;
}

// Convert the channel duty and polarity settings to PWM sequence values.
void pwm_sequence_update(
    nrf_pwm_values_individual_t *sequence,
    volatile uint8_t const *duty,
    uint16_t top_value,
    bool const *active_low) {

    // Individual load mode stores one 16-bit value for each PWM channel.
    sequence->channel_0 = pwm_channel_value(duty[0], top_value, active_low[0]);
    sequence->channel_1 = pwm_channel_value(duty[1], top_value, active_low[1]);
    sequence->channel_2 = pwm_channel_value(duty[2], top_value, active_low[2]);
    sequence->channel_3 = pwm_channel_value(duty[3], top_value, active_low[3]);
}
