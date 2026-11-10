#include <stddef.h>

#include "soft_pwm.h"

// Update the physical output only when its state changes.
static void process_output(
    soft_pwm_t *pwm,
    bool state) {
    if (pwm->output_state == state) return;

    pwm->output_state = state;
    pwm->output(state, pwm->output_context);
}

bool soft_pwm_init(
    soft_pwm_t *pwm,
    uint32_t period_us,
    soft_pwm_output_t output,
    void *output_context) {
    if (pwm == NULL || period_us == 0 || output == NULL) return false;

    pwm->period_us = period_us;
    pwm->on_time_us = 0;
    pwm->output_state = false;
    pwm->output = output;
    pwm->output_context = output_context;

    // Establish the initial time reference for the PWM phase.
    nrfx_systick_get(&pwm->phase_start);

    // Initialize the physical output unconditionally.
    pwm->output(false, pwm->output_context);

    return true;
}

void soft_pwm_set_duty(
    soft_pwm_t *pwm,
    uint8_t duty_percent) {
    if (pwm == NULL) return;

    // Limit the requested duty cycle to the supported range.
    if (duty_percent > SOFT_PWM_PERCENT_MAX) duty_percent = SOFT_PWM_PERCENT_MAX;

    // A zero duty cycle disables the output immediately.
    if (duty_percent == 0) {
        pwm->on_time_us = 0;
        process_output(pwm, false);

        return;
    }

    // A full duty cycle enables the output continuously.
    if (duty_percent == SOFT_PWM_PERCENT_MAX) {
        pwm->on_time_us = pwm->period_us;
        process_output(pwm, true);

        return;
    }

    // Start a new PWM phase when leaving a constant output state.
    // When changing between intermediate duty cycles, preserve the current phase.
    if (pwm->on_time_us == 0 || pwm->on_time_us >= pwm->period_us) nrfx_systick_get(&pwm->phase_start);

    // Calculate the ON time using integer arithmetic.
    pwm->on_time_us =
        ((uint64_t)pwm->period_us * duty_percent) /
        SOFT_PWM_PERCENT_MAX;

    // Ensure a non-zero duty cycle produces at least 1 us of ON time.
    if (pwm->on_time_us == 0) pwm->on_time_us = 1;
}

void soft_pwm_process(soft_pwm_t *pwm) {
    uint32_t timeout_us;

    if (pwm == NULL) return;

    // Constant output states do not require periodic processing.
    if (pwm->on_time_us == 0 || pwm->on_time_us >= pwm->period_us) return;

    // During the ON phase, wait for the ON time to expire.
    // During the OFF phase, wait until the entire PWM period has elapsed.
    timeout_us = pwm->output_state
        ? pwm->on_time_us
        : pwm->period_us;

    if (!nrfx_systick_test(&pwm->phase_start, timeout_us)) return;

    // Toggle the output when the current phase expires.
    process_output(pwm, !pwm->output_state);

    // Start measuring the next PWM period at the rising edge.
    // The falling edge does not change the phase reference.
    if (pwm->output_state) nrfx_systick_get(&pwm->phase_start);
}
