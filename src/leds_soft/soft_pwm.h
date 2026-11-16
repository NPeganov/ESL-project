#ifndef _SOFT_PWM_
#define _SOFT_PWM_

#include <stdbool.h>
#include <stdint.h>

#include "nrfx_systick.h"

#define SOFT_PWM_PERCENT_MAX    100

typedef void (*soft_pwm_output_t)(bool state, void *context);

typedef struct
{
    uint32_t period_us;                // PWM period in microseconds.
    uint32_t on_time_us;               // PWM ON time in microseconds.
    nrfx_systick_state_t phase_start;  // Start time of the current PWM phase.
    bool output_state;                 // Current physical output state.
    soft_pwm_output_t output;          // PWM output callback.
    void *output_context;              // User-defined callback context.
} soft_pwm_t;

// Initialize a software PWM instance.
//
// SysTick must be initialized before calling this function.
// The period must not exceed the maximum timeout supported by nrfx_systick_test().
bool soft_pwm_init(
    soft_pwm_t *pwm,
    uint32_t period_us,
    soft_pwm_output_t output,
    void *output_context);

// Set the PWM duty cycle.
//
// The duty cycle is clamped to the range from 0 to SOFT_PWM_PERCENT_MAX.
void soft_pwm_set_duty(
    soft_pwm_t *pwm,
    uint8_t duty_percent);

// Process the software PWM state.
//
// Call this function regularly to maintain PWM timing.
void soft_pwm_process(soft_pwm_t *pwm);

#endif
