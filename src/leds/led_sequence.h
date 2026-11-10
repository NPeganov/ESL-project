#ifndef _LED_SEQUENCE_H_
#define _LED_SEQUENCE_H_

#include <stdbool.h>
#include <stdint.h>

#include "nrfx_systick.h"

#include "led_controller.h"

// Tracks the pause between two consecutive LED sequences.
typedef enum
{
    INTER_LED_PAUSE_NONE,
    INTER_LED_PAUSE_WAITING,
    INTER_LED_PAUSE_PAUSED
} inter_led_pause_state_t;

// Coordinates LED selection, blink counts and pauses between LEDs.
typedef struct
{
    led_controller_t controller;
    const char *device_id;   // Device ID used to determine blink counts
    uint8_t led_idx;         // Current LED index
    bool prepared;           // Current LED has been prepared
    bool running;            // LED sequence is active

    inter_led_pause_state_t inter_led_pause_state;
    nrfx_systick_state_t inter_led_pause_start;
    uint32_t inter_led_pause_elapsed_ms;
    uint32_t inter_led_pause_duration_ms;
} led_sequence_t;

// Initialize the LED sequence controller.
bool led_sequence_init(
    led_sequence_t *sequence,
    const char *device_id,          // Device ID string, at least LEDS_NUMBER characters
    uint32_t pwm_period_us,         // Software PWM period in microseconds
    uint32_t fade_duration_ms,      // Fade duration in milliseconds
    uint32_t cycle_pause_ms,        // Pause between blink cycles in milliseconds
    uint32_t inter_led_pause_ms);   // Pause between different LEDs in milliseconds

// Pause the LED sequence.
void led_sequence_pause(led_sequence_t *sequence);

// Resume the LED sequence.
void led_sequence_resume(led_sequence_t *sequence);

// Process one step of the LED sequence.
void led_sequence_process(led_sequence_t *sequence);

// Check whether the LED sequence is running.
bool led_sequence_is_running(const led_sequence_t *sequence);

#endif