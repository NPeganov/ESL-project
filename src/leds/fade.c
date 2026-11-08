#include "fade.h"

static uint8_t calculate_duty(const fade_t *fade)
{
    int64_t duty_delta;

    duty_delta =
        (int64_t)fade->target_duty -
        (int64_t)fade->start_duty;

    // Interpolate duty between start and target values proportionally to elapsed time.
    return (uint8_t)(
        (int64_t)fade->start_duty +
        (duty_delta * (int64_t)fade->elapsed_ms) /
        (int64_t)fade->duration_ms);
}

void fade_init(
    fade_t *fade,
    soft_pwm_t *pwm)
{
    if (fade == NULL) return;

    fade->pwm = pwm;
    fade->start_duty = 0U;
    fade->target_duty = 0U;
    fade->duration_ms = 0U;
    fade->elapsed_ms = 0U;
    fade->active = false;
    fade->paused = false;
}

void fade_start(
    fade_t *fade,
    uint8_t start_duty,
    uint8_t target_duty,
    uint32_t duration_ms)
{
    if (fade == NULL || fade->pwm == NULL) return;

    if (start_duty > FADE_PERCENT_MAX) start_duty = FADE_PERCENT_MAX;
    if (target_duty > FADE_PERCENT_MAX) target_duty = FADE_PERCENT_MAX;

    fade->start_duty = start_duty;
    fade->target_duty = target_duty;
    fade->duration_ms = duration_ms;
    fade->elapsed_ms = 0U;
    fade->paused = false;

    if (duration_ms == 0U ||
        start_duty == target_duty)
    {
        soft_pwm_set_duty(fade->pwm, target_duty);
        fade->active = false;

        return;
    }

    soft_pwm_set_duty(fade->pwm, start_duty);
    nrfx_systick_get(&fade->update_time);

    fade->active = true;
}

void fade_stop(fade_t *fade)
{
    if (fade == NULL) return;
    if (!fade->active) return;

    fade->active = false;
    fade->paused = true;
}

void fade_resume(fade_t *fade)
{
    if (fade == NULL || fade->pwm == NULL) return;
    if (!fade->paused) return;

    if (fade->duration_ms == 0U ||
        fade->elapsed_ms >= fade->duration_ms)
    {
        fade->paused = false;
        return;
    }

    nrfx_systick_get(&fade->update_time);

    fade->paused = false;
    fade->active = true;
}

void fade_process(fade_t *fade)
{
    if (fade == NULL || fade->pwm == NULL) return;
    if (!fade->active) return;

    if (fade->duration_ms == 0U)
    {
        soft_pwm_set_duty(
            fade->pwm,
            fade->target_duty);

        fade->active = false;
        fade->paused = false;

        return;
    }

    if (!nrfx_systick_test(
            &fade->update_time,
            FADE_UPDATE_INTERVAL_MS * 1000UL)) return;
    
    nrfx_systick_get(&fade->update_time);

    if (fade->elapsed_ms >= fade->duration_ms ||
        fade->duration_ms - fade->elapsed_ms <= FADE_UPDATE_INTERVAL_MS)
    {
        fade->elapsed_ms = fade->duration_ms;

        soft_pwm_set_duty(fade->pwm, fade->target_duty);
        fade->active = false;
        fade->paused = false;

        return;
    }

    fade->elapsed_ms += FADE_UPDATE_INTERVAL_MS;

    soft_pwm_set_duty(
        fade->pwm,
        calculate_duty(fade));
}

bool fade_is_active(const fade_t *fade)
{
    if (fade == NULL) return false;

    return fade->active;
}

bool fade_is_paused(const fade_t *fade)
{
    if (fade == NULL) return false;

    return fade->paused;
}