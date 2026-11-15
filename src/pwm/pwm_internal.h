#ifndef _PWM_INTERNAL_H_
#define _PWM_INTERNAL_H_

#include <stdbool.h>
#include <stdint.h>

#include "nrfx_pwm.h"

// Hardware timing selected to approximate the requested PWM frequency.
typedef struct {
    // Prescaled clock source selected by the timing calculation.
    nrf_pwm_clk_t base_clock;

    // Number of PWM clock ticks in one output period.
    uint16_t top_value;

    // Actual frequency produced by base_clock and top_value, in hertz.
    uint32_t actual_frequency_hz;
} pwm_timing_t;

// Select PWM clock and period settings for the requested frequency.
// Return false if the requested frequency is outside the supported range.
bool pwm_timing_select(uint32_t frequency_hz, pwm_timing_t *timing);

// Update the PWM sequence buffer using the current duty and polarity settings.
// The duty values may be changed asynchronously by pwm_set().
void pwm_sequence_update(
    nrf_pwm_values_individual_t *sequence,
    volatile uint8_t const *duty,
    uint16_t top_value,
    bool const *active_low);

#endif
