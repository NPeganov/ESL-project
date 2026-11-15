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

// Select the fastest supported base clock that can represent the requested frequency.
// Return false when no supported clock and period count can produce that frequency.
bool pwm_timing_select(uint32_t frequency_hz, pwm_timing_t *timing);

// Recalculate all four sequence values from the current channel duty and polarity settings.
// The sequence buffer is written by the CPU and read by the PWM peripheral.
// The duty array may be updated asynchronously by pwm_set().
void pwm_sequence_update(
    nrf_pwm_values_individual_t *sequence,
    volatile uint8_t const *duty,
    uint16_t top_value,
    bool const *active_low);

#endif
