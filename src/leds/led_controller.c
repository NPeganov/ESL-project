#include <stddef.h>

#include "led_controller.h"
#include "leds.h"

#define LED_CONTROLLER_PAUSE_UPDATE_INTERVAL_MS 1

// Phases of one LED sequence.
enum {
    LED_CONTROLLER_PHASE_FADE_IN,
    LED_CONTROLLER_PHASE_FADE_OUT,
    LED_CONTROLLER_PHASE_PAUSE
};

// Map the logical PWM output state to the physical LED.
static void pwm_led_output(bool state, void *context) {
    led_controller_t *controller = context;

    if (state) led_on(controller->led_idx);
    else led_off(controller->led_idx);
}

// Start the fade-in phase from zero brightness.
static void start_fade_in(led_controller_t *controller) {
    fade_start(
        &controller->fade,
        0,
        FADE_PERCENT_MAX,
        controller->fade_duration_ms);

    controller->phase = LED_CONTROLLER_PHASE_FADE_IN;
}

// Start the fade-out phase from full brightness.
static void start_fade_out(led_controller_t *controller) {
    fade_start(
        &controller->fade,
        FADE_PERCENT_MAX,
        0,
        controller->fade_duration_ms);

    controller->phase = LED_CONTROLLER_PHASE_FADE_OUT;
}

bool led_controller_init(
    led_controller_t *controller,
    uint32_t pwm_period_us,
    uint32_t fade_duration_ms,
    uint32_t pause_duration_ms) {
    if (controller == NULL) return false;

    controller->fade_duration_ms = fade_duration_ms;
    controller->pause_duration_ms = pause_duration_ms;
    controller->pause_elapsed_ms = 0;
    controller->cycle_count = 0;
    controller->cycles_completed = 0;
    controller->led_idx = 0;
    controller->phase = LED_CONTROLLER_PHASE_FADE_IN;
    controller->state = LED_CONTROLLER_STATE_IDLE;
    controller->initialized = false;

    // Initialize software PWM and use the controller as callback context.
    // soft_pwm_init() invokes the callback immediately, so LED 0 is selected
    // before initialization reaches this call.
    if (!soft_pwm_init(
            &controller->pwm,
            pwm_period_us,
            pwm_led_output,
            controller)) return false;

    // Fade changes the PWM duty cycle; PWM handles the physical output.
    fade_init(&controller->fade, &controller->pwm);

    controller->initialized = true;

    return true;
}

void led_controller_prepare(
    led_controller_t *controller,
    uint32_t led_idx,
    uint32_t cycle_count) {
    if (controller == NULL || !controller->initialized) return;
    if (led_idx >= LEDS_NUMBER || cycle_count == 0) return;

    // Turn off the previously selected LED before changing the selection.
    soft_pwm_set_duty(&controller->pwm, 0);

    controller->led_idx = led_idx;

    // Ensure the newly selected LED starts from the off state.
    led_off(controller->led_idx);

    // Replace the current sequence and reset its progress.
    controller->cycle_count = cycle_count;
    controller->cycles_completed = 0;
    controller->pause_elapsed_ms = 0;
    controller->phase = LED_CONTROLLER_PHASE_FADE_IN;

    // Preparation never starts the sequence.
    controller->state = LED_CONTROLLER_STATE_IDLE;
}

void led_controller_pause(led_controller_t *controller) {
    if (controller == NULL || !controller->initialized) return;
    if (controller->state != LED_CONTROLLER_STATE_RUNNING) return;

    // Preserve fade progress if a fade phase is currently active.
    // The inter-cycle pause is handled separately by resume().
    if (fade_is_active(&controller->fade))
        fade_stop(&controller->fade);

    controller->state = LED_CONTROLLER_STATE_PAUSED;
}

void led_controller_resume(led_controller_t *controller) {
    if (controller == NULL || !controller->initialized) return;
    if (controller->state == LED_CONTROLLER_STATE_RUNNING) return;

    // A sequence must be prepared before it can be started.
    if (controller->cycle_count == 0) return;

    if (controller->state == LED_CONTROLLER_STATE_IDLE) {
        // Start a prepared or previously completed sequence from the beginning.
        controller->cycles_completed = 0;
        controller->pause_elapsed_ms = 0;
        start_fade_in(controller);
    }
    else if (controller->phase == LED_CONTROLLER_PHASE_PAUSE) {
        // Restart the entire inter-cycle pause after resuming.
        controller->pause_elapsed_ms = 0;
        nrfx_systick_get(&controller->pause_start);
    }
    else
        // Continue the interrupted fade from its saved progress.
        fade_resume(&controller->fade);

    controller->state = LED_CONTROLLER_STATE_RUNNING;
}

bool led_controller_process(led_controller_t *controller) {
    if (controller == NULL || !controller->initialized) return false;

    // PWM must keep running even when the sequence is paused.
    // This preserves the current average brightness of the LED.
    soft_pwm_process(&controller->pwm);

    if (controller->state != LED_CONTROLLER_STATE_RUNNING) return false;

    switch (controller->phase) {
        case LED_CONTROLLER_PHASE_FADE_IN:
            fade_process(&controller->fade);

            // Once fade-in is complete, immediately begin fade-out.
            if (!fade_is_active(&controller->fade) &&
                !fade_is_paused(&controller->fade))
                start_fade_out(controller);

            break;

        case LED_CONTROLLER_PHASE_FADE_OUT:
            fade_process(&controller->fade);

            // A completed fade-out marks the end of one full cycle.
            if (!fade_is_active(&controller->fade) &&
                !fade_is_paused(&controller->fade)) {
                controller->cycles_completed++;

                // The sequence is complete when all requested cycles finish.
                // Fade-out has already brought the LED to zero brightness.
                if (controller->cycles_completed >= controller->cycle_count) {
                    controller->state = LED_CONTROLLER_STATE_IDLE;
                    return true;
                }

                // Start the next cycle immediately if no inter-cycle pause
                // has been configured.
                if (controller->pause_duration_ms == 0) {
                    start_fade_in(controller);
                    break;
                }

                // Otherwise, begin measuring the inter-cycle pause.
                nrfx_systick_get(&controller->pause_start);
                controller->pause_elapsed_ms = 0;
                controller->phase = LED_CONTROLLER_PHASE_PAUSE;
            }

            break;

        case LED_CONTROLLER_PHASE_PAUSE:
            // Update the pause timer at the controller's polling interval.
            if (!nrfx_systick_test(
                    &controller->pause_start,
                    LED_CONTROLLER_PAUSE_UPDATE_INTERVAL_MS * 1000)) break;

            nrfx_systick_get(&controller->pause_start);

            controller->pause_elapsed_ms += LED_CONTROLLER_PAUSE_UPDATE_INTERVAL_MS;

            // Start the next fade-in when the inter-cycle pause expires.
            if (controller->pause_elapsed_ms >= controller->pause_duration_ms) {
                controller->pause_elapsed_ms = 0;
                start_fade_in(controller);
            }

            break;

        default:
            // An invalid phase is an internal error; leave the LED safely off.
            soft_pwm_set_duty(&controller->pwm, 0);
            controller->state = LED_CONTROLLER_STATE_IDLE;
            return true;
    }

    return false;
}

bool led_controller_is_running(const led_controller_t *controller) {
    if (controller == NULL || !controller->initialized) return false;

    return controller->state == LED_CONTROLLER_STATE_RUNNING;
}