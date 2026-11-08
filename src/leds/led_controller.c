#include "led_controller.h"

#include "leds.h"

enum
{
    LED_CONTROLLER_PHASE_FADE_IN,
    LED_CONTROLLER_PHASE_FADE_OUT,
    LED_CONTROLLER_PHASE_PAUSE
};

static void pwm_led_output(bool state, void *context)
{
    led_controller_t *controller = context;

    if (state) led_on(controller->led_idx);
    else led_off(controller->led_idx);
}

static void start_fade_in(led_controller_t *controller)
{
    // Start fade in.
    fade_start(
        &controller->fade,              // Fade instance.
        0,                              // Initial duty cycle.
        FADE_PERCENT_MAX,               // Target duty cycle.
        controller->fade_duration_ms);  // Fade duration.

    controller->phase = LED_CONTROLLER_PHASE_FADE_IN;
}

static void start_fade_out(led_controller_t *controller)
{
    // Start fade out.
    fade_start(
        &controller->fade,              // Fade instance.
        FADE_PERCENT_MAX,               // Initial duty cycle.
        0,                              // Target duty cycle.
        controller->fade_duration_ms);  // Fade duration.

    controller->phase = LED_CONTROLLER_PHASE_FADE_OUT;
}

bool led_controller_init(
    led_controller_t *controller,
    uint32_t pwm_period_us,
    uint32_t fade_duration_ms,
    uint32_t pause_duration_ms)
{
    if (controller == NULL) return false;

    controller->fade_duration_ms = fade_duration_ms;
    controller->pause_duration_ms = pause_duration_ms;
    controller->pause_elapsed_ms = 0;
    controller->cycle_count = 0;
    controller->cycles_completed = 0;
    controller->led_idx = 0;
    controller->phase = LED_CONTROLLER_PHASE_FADE_IN;
    controller->initialized = false;
    controller->active = false;
    controller->paused = false;

    if (!soft_pwm_init(
            &controller->pwm,
            pwm_period_us,
            pwm_led_output,
            controller)) return false;

    fade_init(&controller->fade, &controller->pwm);

    controller->initialized = true;

    return true;
}

void led_controller_start(
    led_controller_t *controller,
    uint32_t led_idx,
    uint32_t cycle_count)
{
    if (controller == NULL || !controller->initialized) return;
    if (led_idx >= LEDS_NUMBER || cycle_count == 0) return;

    // Turn off the previously selected LED before changing its index.
    soft_pwm_set_duty(&controller->pwm, 0);

    controller->led_idx = led_idx;

    // Ensure the newly selected LED is turned off.
    led_off(controller->led_idx);

    controller->cycle_count = cycle_count;
    controller->cycles_completed = 0;
    controller->pause_elapsed_ms = 0;
    controller->paused = false;
    controller->active = true;

    start_fade_in(controller);
}

void led_controller_pause(led_controller_t *controller)
{
    if (controller == NULL || !controller->initialized) return;
    if (!controller->active || controller->paused) return;

    if (fade_is_active(&controller->fade)) fade_stop(&controller->fade);

    controller->paused = true;
}

void led_controller_resume(led_controller_t *controller)
{
    if (controller == NULL || !controller->initialized) return;
    if (!controller->active || !controller->paused) return;

    if (controller->phase == LED_CONTROLLER_PHASE_PAUSE)
    {
        // Restart the pause interval after resuming.
        controller->pause_elapsed_ms = 0;
        nrfx_systick_get(&controller->pause_start);
    }
    else
        fade_resume(&controller->fade);

    controller->paused = false;
}

void led_controller_process(led_controller_t *controller)
{
    if (controller == NULL || !controller->initialized) return;

    soft_pwm_process(&controller->pwm);

    if (!controller->active || controller->paused) return;

    switch (controller->phase)
    {
        case LED_CONTROLLER_PHASE_FADE_IN:
            fade_process(&controller->fade);

            if (!fade_is_active(&controller->fade) &&
                !fade_is_paused(&controller->fade))
            {
                start_fade_out(controller);
            }

            break;

        case LED_CONTROLLER_PHASE_FADE_OUT:
            fade_process(&controller->fade);

            if (!fade_is_active(&controller->fade) &&
                !fade_is_paused(&controller->fade))
            {
                controller->cycles_completed++;

                if (controller->cycles_completed >= controller->cycle_count)
                {
                    controller->active = false;
                    break;
                }

                if (controller->pause_duration_ms == 0)
                {
                    start_fade_in(controller);
                    break;
                }

                nrfx_systick_get(&controller->pause_start);
                controller->pause_elapsed_ms = 0;
                controller->phase = LED_CONTROLLER_PHASE_PAUSE;
            }

            break;

        case LED_CONTROLLER_PHASE_PAUSE:
            if (!nrfx_systick_test(
                    &controller->pause_start,
                    FADE_UPDATE_INTERVAL_MS * 1000)) break;

            nrfx_systick_get(&controller->pause_start);

            controller->pause_elapsed_ms += FADE_UPDATE_INTERVAL_MS;

            if (controller->pause_elapsed_ms >= controller->pause_duration_ms)
            {
                controller->pause_elapsed_ms = 0;
                start_fade_in(controller);
            }

            break;

        default:
            controller->active = false;
            break;
    }
}

bool led_controller_is_active(const led_controller_t *controller)
{
    if (controller == NULL || !controller->initialized) return false;

    return controller->active;
}