#include <ctype.h>
#include <string.h>

#include "nrfx_systick.h"

#include "leds.h"
#include "led_sequence.h"

#define INTER_LED_PAUSE_UPDATE_INTERVAL_MS   1

// Start a new pause between LED sequences.
static void inter_led_pause_start(led_sequence_t *sequence)
{
    sequence->inter_led_pause_elapsed_ms = 0;
    nrfx_systick_get(&sequence->inter_led_pause_start);
    sequence->inter_led_pause_state = INTER_LED_PAUSE_WAITING;
}

// Pause the current inter-LED interval.
static void inter_led_pause_pause(led_sequence_t *sequence)
{
    sequence->inter_led_pause_state = INTER_LED_PAUSE_PAUSED;
}

// Resume the inter-LED interval from the beginning.
static void inter_led_pause_resume(led_sequence_t *sequence)
{
    inter_led_pause_start(sequence);
}

// Process the inter-LED interval.
static bool inter_led_pause_process(led_sequence_t *sequence)
{
    if (sequence->inter_led_pause_state != INTER_LED_PAUSE_WAITING)
        return false;

    // Update elapsed time in 1 ms steps.
    if (!nrfx_systick_test(
            &sequence->inter_led_pause_start,
            INTER_LED_PAUSE_UPDATE_INTERVAL_MS * 1000))
        return false;

    nrfx_systick_get(&sequence->inter_led_pause_start);
    sequence->inter_led_pause_elapsed_ms += INTER_LED_PAUSE_UPDATE_INTERVAL_MS;

    if (sequence->inter_led_pause_elapsed_ms < sequence->inter_led_pause_duration_ms)
        return false;

    sequence->inter_led_pause_state = INTER_LED_PAUSE_NONE;
    sequence->inter_led_pause_elapsed_ms = 0;

    return true;
}

// Advance to the next LED and wrap around at the end of the LED array.
static void next_led(led_sequence_t *sequence)
{
    ++sequence->led_idx;

    if (sequence->led_idx >= LEDS_NUMBER)
        sequence->led_idx = 0;
}

// Prepare the current LED using its blink count from the device ID.
static void prepare_current_led(led_sequence_t *sequence)
{
    uint8_t blink_count = 0;

    if (isdigit((unsigned char)sequence->device_id[sequence->led_idx]))
        blink_count = (uint8_t)(sequence->device_id[sequence->led_idx] - '0');

    if (blink_count == 0) {
        next_led(sequence);
        return;
    }

    led_controller_prepare(
        &sequence->controller,
        sequence->led_idx,
        blink_count);

    sequence->prepared = true;
    led_controller_resume(&sequence->controller);
}

// Initialize the LED sequence controller.
bool led_sequence_init(
    led_sequence_t *sequence,
    const char *device_id,
    uint32_t pwm_period_us,
    uint32_t fade_duration_ms,
    uint32_t cycle_pause_ms,
    uint32_t inter_led_pause_ms)
{
    if (device_id == NULL || strlen(device_id) < LEDS_NUMBER)
        return false;

    if (!led_controller_init(
            &sequence->controller,
            pwm_period_us,
            fade_duration_ms,
            cycle_pause_ms))
        return false;

    sequence->device_id = device_id;
    sequence->led_idx = 0;
    sequence->prepared = false;
    sequence->running = false;

    sequence->inter_led_pause_state = INTER_LED_PAUSE_NONE;
    sequence->inter_led_pause_elapsed_ms = 0;
    sequence->inter_led_pause_duration_ms = inter_led_pause_ms;

    return true;
}

// Pause the LED sequence.
void led_sequence_pause(led_sequence_t *sequence)
{
    if (!sequence->running)
        return;

    if (sequence->inter_led_pause_state == INTER_LED_PAUSE_WAITING)
        inter_led_pause_pause(sequence);
    else if (sequence->prepared)
        led_controller_pause(&sequence->controller);

    sequence->running = false;
}

// Resume the LED sequence.
void led_sequence_resume(led_sequence_t *sequence)
{
    if (sequence->running)
        return;

    if (sequence->inter_led_pause_state == INTER_LED_PAUSE_PAUSED)
        inter_led_pause_resume(sequence);
    else if (sequence->prepared)
        led_controller_resume(&sequence->controller);

    sequence->running = true;
}

// Process one step of the LED sequence.
void led_sequence_process(led_sequence_t *sequence)
{
    bool sequence_completed = led_controller_process(&sequence->controller);

    if (!sequence->running)
        return;

    // Move to the next LED after the current sequence completes.
    if (sequence_completed) {
        next_led(sequence);
        sequence->prepared = false;
        inter_led_pause_start(sequence);
    }

    // Wait until the inter-LED pause has elapsed.
    if (sequence->inter_led_pause_state != INTER_LED_PAUSE_NONE) {
        if (!inter_led_pause_process(sequence))
            return;
    }

    // Prepare the current LED if it has not been prepared yet.
    if (!sequence->prepared)
        prepare_current_led(sequence);
}

// Check whether the LED sequence is running.
bool led_sequence_is_running(const led_sequence_t *sequence)
{
    return sequence->running;
}