#include <stdbool.h>
#include <stdint.h>

#include "nrfx_pwm.h"

#include "pwm.h"
#include "pwm_internal.h"

// Bit 15 selects falling-edge polarity for an active-low PWM output.
#define PWM_VALUE_POLARITY_MASK 0x8000

// Convert one channel's duty cycle and active level to a PWM sequence value.
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

// Rebuild the four hardware sequence values from the current channel settings.
// This module owns the conversion from logical duty and polarity to hardware encoding.
void pwm_sequence_update(
    nrf_pwm_values_individual_t *sequence,
    volatile uint8_t const *duty,
    uint16_t top_value,
    bool const *active_low) {

    // Individual load mode stores one 16-bit value for each PWM channel.
    sequence->channel_0 = pwm_channel_value(
        duty[PWM_CHANNEL_0], top_value, active_low[PWM_CHANNEL_0]);
    sequence->channel_1 = pwm_channel_value(
        duty[PWM_CHANNEL_1], top_value, active_low[PWM_CHANNEL_1]);
    sequence->channel_2 = pwm_channel_value(
        duty[PWM_CHANNEL_2], top_value, active_low[PWM_CHANNEL_2]);
    sequence->channel_3 = pwm_channel_value(
        duty[PWM_CHANNEL_3], top_value, active_low[PWM_CHANNEL_3]);
}
