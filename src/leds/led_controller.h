#ifndef _LED_CONTROLLER_
#define _LED_CONTROLLER_

#include <stdbool.h>
#include <stdint.h>

#include "nrfx_systick.h"
#include "fade.h"
#include "soft_pwm.h"

// LED controller operating states.
typedef enum {
    // A sequence is prepared and waiting to be started, or has completed.
    LED_CONTROLLER_STATE_IDLE,

    // The sequence is being executed.
    LED_CONTROLLER_STATE_RUNNING,

    // The sequence is suspended, preserving its progress.
    LED_CONTROLLER_STATE_PAUSED
} led_controller_state_t;

// LED sequence controller instance.
typedef struct {
    soft_pwm_t pwm;                     // Software PWM instance.
    fade_t fade;                        // Fade instance.

    nrfx_systick_state_t pause_start;   // Start time of the current inter-cycle pause.

    uint32_t fade_duration_ms;          // Duration of each fade phase.
    uint32_t pause_duration_ms;         // Pause between fade cycles.
    uint32_t pause_elapsed_ms;          // Elapsed time in the current inter-cycle pause.

    uint32_t cycle_count;               // Total number of fade-in/fade-out cycles.
    uint32_t cycles_completed;          // Number of completed fade-in/fade-out cycles.

    uint32_t led_idx;                   // LED controlled by this instance.
    uint8_t phase;                      // Current phase of the sequence.

    led_controller_state_t state;       // Current operating state.
    bool initialized;                   // True after successful initialization.
} led_controller_t;

// Initialize the LED controller, software PWM and fade modules.
//
// Preconditions:
// - leds_init() has been called.
// - nrfx_systick_init() has been called.
// - pwm_period_us is within the timeout range supported by nrfx_systick_test().
//
// Initialization sets the controller to IDLE. No sequence is prepared or started.
bool led_controller_init(
    led_controller_t *controller,
    uint32_t pwm_period_us,
    uint32_t fade_duration_ms,
    uint32_t pause_duration_ms);

// Prepare a new LED sequence.
//
// Selects the LED, resets the cycle counter and sets the controller to IDLE.
// This replaces any previously prepared, running or paused sequence.
// The sequence is not started until led_controller_resume() is called.
//
// Invalid arguments or an uninitialized controller are ignored.
void led_controller_prepare(
    led_controller_t *controller,
    uint32_t led_idx,
    uint32_t cycle_count);

// Pause the currently running sequence.
//
// If a fade phase is active, its progress is preserved.
// If the controller is in the inter-cycle pause phase, that pause is
// suspended and will restart from the beginning when resumed.
//
// Calls made when the controller is IDLE or already PAUSED have no effect.
void led_controller_pause(led_controller_t *controller);

// Start or resume the prepared LED sequence.
//
// A successful led_controller_prepare() call is required before the first
// call to this function.
// In IDLE, starts the prepared sequence from the first fade-in phase.
// After normal completion, calling this function again restarts the same
// sequence with its previously configured parameters.
// In PAUSED, continues from the saved phase and progress.
// In RUNNING, has no effect.
void led_controller_resume(led_controller_t *controller);

// Process the current LED sequence.
//
// Advances the active fade or inter-cycle pause phase.
// Software PWM is processed regardless of the controller state so that
// the current brightness is maintained while the sequence is paused.
//
// When all requested cycles are complete, the controller returns to IDLE.
//
// Returns true once when the sequence completes, otherwise false.
bool led_controller_process(led_controller_t *controller);

// Check whether the controller is running.
//
// Returns true if the controller is in RUNNING state, otherwise false.
bool led_controller_is_running(const led_controller_t *controller);

#endif