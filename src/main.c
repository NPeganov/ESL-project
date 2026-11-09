#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#include "nrfx_systick.h"

#include "btn/btn.h"
#include "leds/leds.h"
#include "leds/led_controller.h"

#define DEVICE_ID                           "6609"

#define PWM_PERIOD_US                       1000
#define FADE_DURATION_MS                    800
#define CYCLE_PAUSE_MS                      500

#define LED_TRANSITION_PAUSE_MS             500
#define LED_TRANSITION_UPDATE_INTERVAL_MS   1


typedef enum
{
    LED_TRANSITION_NONE,
    LED_TRANSITION_WAITING,
    LED_TRANSITION_PAUSED
} led_transition_state_t;


typedef struct
{
    nrfx_systick_state_t start;
    uint16_t elapsed_ms;
    led_transition_state_t state;
} led_transition_t;


/**
 * @brief Start the delay between two LED sequences.
 *
 * @param[in,out] transition Transition timer state.
 */
static void transition_pause_start(led_transition_t *transition)
{
    transition->elapsed_ms = 0;
    nrfx_systick_get(&transition->start);
    transition->state = LED_TRANSITION_WAITING;
}


/**
 * @brief Pause the delay between two LED sequences.
 *
 * @param[in,out] transition Transition timer state.
 */
static void transition_pause_pause(led_transition_t *transition)
{
    transition->state = LED_TRANSITION_PAUSED;
}


/**
 * @brief Resume the delay between two LED sequences.
 *
 * A full delay interval starts again after resuming.
 *
 * @param[in,out] transition Transition timer state.
 */
static void transition_pause_resume(led_transition_t *transition)
{
    transition_pause_start(transition);
}


/**
 * @brief Process the delay between two LED sequences.
 *
 * @param[in,out] transition Transition timer state.
 *
 * @return true if the delay has elapsed.
 */
static bool transition_pause_process(led_transition_t *transition)
{
    if (transition->state != LED_TRANSITION_WAITING)
        return false;

    // Update elapsed time in 1 ms steps.
    if (!nrfx_systick_test(
            &transition->start,
            LED_TRANSITION_UPDATE_INTERVAL_MS * 1000))
        return false;

    nrfx_systick_get(&transition->start);
    transition->elapsed_ms += LED_TRANSITION_UPDATE_INTERVAL_MS;

    if (transition->elapsed_ms < LED_TRANSITION_PAUSE_MS)
        return false;

    transition->state = LED_TRANSITION_NONE;
    transition->elapsed_ms = 0;

    return true;
}


int main(void)
{
    const char dev_id[] = DEVICE_ID;

    led_controller_t controller;
    led_transition_t transition = {0};

    uint8_t led_idx = 0;
    uint8_t blink_count = 0;

    leds_init();
    leds_off();

    nrfx_systick_init();

    btn_init();

    if (!led_controller_init(
            &controller,
            PWM_PERIOD_US,
            FADE_DURATION_MS,
            CYCLE_PAUSE_MS)) while (true);

    while (true)
    {
        // Wrap the LED index when the end of the LED array is reached.
        if (led_idx >= LEDS_NUMBER)
            led_idx = 0;

        // Reset the blink count before reading the current device ID character.
        blink_count = 0;

        // Read the blink count directly from the current device ID character.
        if (isdigit((unsigned char)dev_id[led_idx]))
            blink_count = (uint8_t)(dev_id[led_idx] - '0');

        // Prepare the current LED only when it has a non-zero blink count.
        if (blink_count > 0)
            led_controller_prepare(&controller, led_idx, blink_count);

        // Advance to the next LED. Its upper bound is checked at the
        // beginning of the next main-loop iteration.
        ++led_idx;

        bool sequence_completed = false;

        /*
         * Process the current LED sequence. When blink_count is zero,
         * there is no sequence to run, but one iteration is still needed
         * to service the controller, button events and transition pause.
         */
        while (true)
        {
            // Keep the software PWM serviced on every iteration.
            sequence_completed = led_controller_process(&controller);

            // Process all pending button events.
            btn_event_t event;
            while ((event = btn_event_take()) != BTN_EVENT_NONE)
            {
                if (event != BTN_EVENT_DOUBLE_PRESS) continue;

                // A double press during the transition pause controls
                // the transition pause itself.
                if (transition.state == LED_TRANSITION_WAITING)
                    transition_pause_pause(&transition);
                else if (transition.state == LED_TRANSITION_PAUSED)
                    transition_pause_resume(&transition);
                // Otherwise, control the current LED sequence.
                else if (blink_count > 0 && led_controller_is_running(&controller))
                    led_controller_pause(&controller);
                else if (blink_count > 0)
                    led_controller_resume(&controller);
            }

            // Resume the LED sequence when the transition pause expires.
            if (transition_pause_process(&transition))
                led_controller_resume(&controller);

            // A configured LED leaves this loop after its sequence completes.
            // An unconfigured LED leaves after one processing iteration.
            if (sequence_completed || blink_count == 0)
                break;
        }

        // Start the transition pause only after a real LED sequence
        // has completed.
        if (sequence_completed)
            transition_pause_start(&transition);
    }
}