#include "fade.h"

// Calculate the current duty cycle using linear interpolation.
static uint8_t calculate_duty(const fade_t *fade) {
    int64_t duty_delta;

    duty_delta =
        (int64_t)fade->target_duty -
        (int64_t)fade->start_duty;

    return (uint8_t)(
        (int64_t)fade->start_duty +
        (duty_delta * (int64_t)fade->elapsed_ms) /
        (int64_t)fade->duration_ms);
}

void fade_init(
    fade_t *fade,
    soft_pwm_t *pwm) {
    if (fade == NULL || pwm == NULL) return;

    fade->pwm = pwm;
    fade->start_duty = 0;
    fade->target_duty = 0;
    fade->duration_ms = 0;
    fade->elapsed_ms = 0;
    fade->active = false;
    fade->paused = false;
}

void fade_start(
    fade_t *fade,
    uint8_t start_duty,
    uint8_t target_duty,
    uint32_t duration_ms) {
    if (fade == NULL || fade->pwm == NULL) return;

    // Limit duty cycle values to the supported percentage range.
    if (start_duty > FADE_PERCENT_MAX) start_duty = FADE_PERCENT_MAX;
    if (target_duty > FADE_PERCENT_MAX) target_duty = FADE_PERCENT_MAX;

    fade->start_duty = start_duty;
    fade->target_duty = target_duty;
    fade->duration_ms = duration_ms;
    fade->elapsed_ms = 0;
    fade->paused = false;

    // Handle zero-duration transitions and transitions without a duty change immediately.
    if (duration_ms == 0 || start_duty == target_duty) {
        soft_pwm_set_duty(fade->pwm, target_duty);
        fade->active = false;

        return;
    }

    // Apply the initial duty cycle before starting the fade.
    soft_pwm_set_duty(fade->pwm, start_duty);

    // Establish the time reference for the first update interval.
    nrfx_systick_get(&fade->update_time);

    fade->active = true;
}

void fade_stop(fade_t *fade) {
    if (fade == NULL) return;
    if (!fade->active) return;

    // Pause the fade without changing the current PWM duty cycle.
    fade->active = false;
    fade->paused = true;
}

void fade_resume(fade_t *fade) {
    if (fade == NULL || fade->pwm == NULL) return;
    if (!fade->paused) return;

    // Do not resume a fade that has already reached its target.
    if (fade->duration_ms == 0 || fade->elapsed_ms >= fade->duration_ms) {
        fade->paused = false;
        return;
    }

    // Restart the update interval so that the pause duration is excluded.
    nrfx_systick_get(&fade->update_time);

    fade->paused = false;
    fade->active = true;
}

void fade_process(fade_t *fade) {
    if (fade == NULL || fade->pwm == NULL) return;
    if (!fade->active) return;

    // Complete the transition immediately if its duration is zero.
    if (fade->duration_ms == 0) {
        soft_pwm_set_duty(fade->pwm, fade->target_duty);

        fade->active = false;
        fade->paused = false;

        return;
    }

    // Wait until the next update interval has elapsed.
    if (!nrfx_systick_test(
            &fade->update_time,
            FADE_UPDATE_INTERVAL_MS * 1000)) return;

    // Start measuring the next update interval.
    nrfx_systick_get(&fade->update_time);

    // Complete the fade when the remaining progress fits into one update step.
    if (fade->elapsed_ms >= fade->duration_ms ||
        fade->duration_ms - fade->elapsed_ms <= FADE_UPDATE_INTERVAL_MS) {
        fade->elapsed_ms = fade->duration_ms;

        soft_pwm_set_duty(fade->pwm, fade->target_duty);
        fade->active = false;
        fade->paused = false;

        return;
    }

    // Advance by one processed update step.
    // Delayed calls do not cause missed steps to be compensated.
    fade->elapsed_ms += FADE_UPDATE_INTERVAL_MS;

    // Apply the duty cycle corresponding to the current fade progress.
    soft_pwm_set_duty(
        fade->pwm,
        calculate_duty(fade));
}

bool fade_is_active(const fade_t *fade) {
    if (fade == NULL) return false;

    return fade->active;
}

bool fade_is_paused(const fade_t *fade) {
    if (fade == NULL) return false;

    return fade->paused;
}
