#include <stdint.h>

#include "nrfx_pwm.h"

#include "pwm_internal.h"

// Convert one channel's duty value to the corresponding PWM sequence value.
static uint16_t pwm_channel_value(
    uint8_t duty,
    uint16_t top_value,
    uint16_t polarity_mask) {

    // Convert the 0..255 duty value to active PWM ticks, rounded to nearest.
    uint32_t active_ticks =
        ((uint32_t)duty * top_value + UINT8_MAX / 2) / UINT8_MAX;

    // The compare value is the inactive part of the period; bit 15 selects polarity.
    uint16_t value = top_value - active_ticks;
    return value | polarity_mask;
}

// Refresh the compare values for all PWM channels from the current duty values.
void pwm_sequence_update(
    nrf_pwm_values_individual_t *sequence,
    volatile uint8_t const *duty,
    uint16_t top_value,
    uint16_t const *polarity_mask) {

    // NRF_PWM_LOAD_INDIVIDUAL stores one value for each of the four channels.
    sequence->channel_0 = pwm_channel_value(
        duty[PWM_CHANNEL_0], top_value, polarity_mask[PWM_CHANNEL_0]);
    sequence->channel_1 = pwm_channel_value(
        duty[PWM_CHANNEL_1], top_value, polarity_mask[PWM_CHANNEL_1]);
    sequence->channel_2 = pwm_channel_value(
        duty[PWM_CHANNEL_2], top_value, polarity_mask[PWM_CHANNEL_2]);
    sequence->channel_3 = pwm_channel_value(
        duty[PWM_CHANNEL_3], top_value, polarity_mask[PWM_CHANNEL_3]);
}
