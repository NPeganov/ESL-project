#ifndef _FADE_
#define _FADE_

#include <stdbool.h>
#include <stdint.h>

#include "nrfx_systick.h"
#include "soft_pwm.h"

#define FADE_UPDATE_INTERVAL_MS    1
#define FADE_PERCENT_MAX           100

// Maintains the timing and progress of a PWM brightness transition.
typedef struct {
    soft_pwm_t *pwm;                    // Software PWM instance
    uint8_t start_duty;                 // Initial duty cycle in percent
    uint8_t target_duty;                // Target duty cycle in percent
    uint32_t duration_ms;               // Fade duration in processed milliseconds
    uint32_t elapsed_ms;                // Accumulated fade progress
    nrfx_systick_state_t update_time;   // Timestamp of the last update
    bool active;                        // Fade is active
    bool paused;                        // Fade is paused
} fade_t;

// Initialize a fade instance.
void fade_init(
    fade_t *fade,
    soft_pwm_t *pwm);

// Start a fade operation.
void fade_start(
    fade_t *fade,
    uint8_t start_duty,     // Initial duty cycle in percent
    uint8_t target_duty,    // Target duty cycle in percent
    uint32_t duration_ms);  // Fade duration in milliseconds

// Stop the active fade while preserving its progress.
void fade_stop(fade_t *fade);

// Resume a previously stopped fade operation.
void fade_resume(fade_t *fade);

// Process the active fade.
void fade_process(fade_t *fade);

// Check whether the fade is active.
bool fade_is_active(const fade_t *fade);

// Check whether the fade is paused.
bool fade_is_paused(const fade_t *fade);

#endif
