#ifndef _SOFT_PWM_H_
#define _SOFT_PWM_H_

#include <stdbool.h>
#include <stdint.h>

#include "nrfx_systick.h"

#define SOFT_PWM_PERCENT_MAX    100U

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

/**
 * @brief Initialize a software PWM instance.
 *
 * @param pwm PWM instance to initialize.
 * @param period_us PWM period in microseconds.
 * @param output PWM output callback.
 * @param output_context User-defined callback context.
 *
 * @return true if initialization was successful, otherwise false.
 */
bool soft_pwm_init(
    soft_pwm_t *pwm,
    uint32_t period_us,
    soft_pwm_output_t output,
    void *output_context);

/**
 * @brief Set the PWM duty cycle.
 *
 * @param pwm PWM instance.
 * @param duty_percent Duty cycle in percent.
 */
void soft_pwm_set_duty(
    soft_pwm_t *pwm,
    uint8_t duty_percent);

/**
 * @brief Process the software PWM state.
 *
 * @param pwm PWM instance.
 */
void soft_pwm_process(soft_pwm_t *pwm);

#endif