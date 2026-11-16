#ifndef _PWM_INTERNAL_H_
#define _PWM_INTERNAL_H_

#include <stdbool.h>
#include <stdint.h>

#include "nrfx_pwm.h"

// Hardware timing selected for the requested PWM frequency.
typedef struct {
    nrf_pwm_clk_t base_clock;
    uint16_t top_value;
    uint32_t actual_frequency_hz;
} pwm_timing_t;

// Select a supported base clock and period count; return false if unavailable.
bool pwm_timing_select(uint32_t frequency_hz, pwm_timing_t *timing);

// Recalculate the four sequence compare values from the current duty settings.
void pwm_sequence_update(
    nrf_pwm_values_individual_t *sequence,
    volatile uint8_t const *duty,
    uint16_t top_value,
    bool const *active_low);

#endif
