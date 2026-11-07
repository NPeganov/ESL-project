#include "soft_pwm.h"

static void process_output(
    soft_pwm_t *pwm,
    bool state)
{
    if (pwm->output_state == state) return;

    pwm->output_state = state;
    pwm->output(state, pwm->output_context);
}

bool soft_pwm_init(
    soft_pwm_t *pwm,
    uint32_t period_us,
    soft_pwm_output_t output,
    void *output_context)
{
    if (pwm == NULL || period_us == 0U || output == NULL) return false;

    pwm->period_us = period_us;
    pwm->on_time_us = 0U;
    pwm->output_state = false;
    pwm->output = output;
    pwm->output_context = output_context;

    nrfx_systick_get(&pwm->phase_start);

    /*
     * Initialize the physical output directly because the initial
     * logical state is already known and must be applied unconditionally.
     */
    pwm->output(false, pwm->output_context);

    return true;
}

void soft_pwm_set_duty(
    soft_pwm_t *pwm,
    uint8_t duty_percent)
{
    if (pwm == NULL) return;

    if (duty_percent > SOFT_PWM_PERCENT_MAX) duty_percent = SOFT_PWM_PERCENT_MAX;

    if (duty_percent == 0U)
    {
        pwm->on_time_us = 0U;
        process_output(pwm, false);

        return;
    }

    if (duty_percent == SOFT_PWM_PERCENT_MAX)
    {
        pwm->on_time_us = pwm->period_us;
        process_output(pwm, true);

        return;
    }

    // Start a new PWM phase when leaving a constant output state.
    if (pwm->on_time_us == 0U || pwm->on_time_us >= pwm->period_us) nrfx_systick_get(&pwm->phase_start);

    pwm->on_time_us =
        ((uint64_t)pwm->period_us * duty_percent) /
        SOFT_PWM_PERCENT_MAX;

    // Ensure a non-zero duty cycle produces at least 1 us of ON time.
    if (pwm->on_time_us == 0U) pwm->on_time_us = 1U;
}

void soft_pwm_process(soft_pwm_t *pwm)
{
    uint32_t timeout_us;

    if (pwm == NULL) return;

    if (pwm->on_time_us == 0U || pwm->on_time_us >= pwm->period_us) return;
    
    timeout_us = pwm->output_state
        ? pwm->on_time_us
        : pwm->period_us;

    if (!nrfx_systick_test(&pwm->phase_start, timeout_us)) return;

    process_output(pwm, !pwm->output_state);

    if (pwm->output_state) nrfx_systick_get(&pwm->phase_start);
}