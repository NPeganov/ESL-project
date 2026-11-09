#ifndef _LED_CONTROLLER_H_
#define _LED_CONTROLLER_H_

#include <stdbool.h>
#include <stdint.h>

#include "nrfx_systick.h"
#include "fade.h"
#include "soft_pwm.h"

/**
 * @brief LED controller operating states.
 */
typedef enum
{
    // A sequence is prepared and waiting to be started.
    LED_CONTROLLER_STATE_IDLE,

    // The sequence is being executed.
    LED_CONTROLLER_STATE_RUNNING,

    // The sequence is suspended, preserving its progress.
    LED_CONTROLLER_STATE_PAUSED
} led_controller_state_t;

/**
 * @brief LED sequence controller instance.
 */
typedef struct
{
    soft_pwm_t pwm;                     // Software PWM instance.
    fade_t fade;                        // Fade instance.

    nrfx_systick_state_t pause_start;   // Start time of the current inter-cycle pause.

    uint32_t fade_duration_ms;          // Duration of each fade phase.
    uint32_t pause_duration_ms;         // Duration of the pause between fade cycles.
    uint32_t pause_elapsed_ms;          // Elapsed time in the current inter-cycle pause.

    uint32_t cycle_count;               // Total number of fade-in/fade-out cycles.
    uint32_t cycles_completed;          // Number of completed fade-in/fade-out cycles.

    uint32_t led_idx;                   // LED controlled by this instance.
    uint8_t phase;                      // Current phase of the sequence.

    led_controller_state_t state;      // Current operating state.
    bool initialized;                   // True after successful initialization.
} led_controller_t;

/**
 * @brief Initializes an LED controller instance.
 *
 * Initializes the software PWM and fade modules and sets the controller
 * to IDLE. No LED sequence is prepared or started.
 *
 * @param[out] controller LED controller instance.
 * @param[in] pwm_period_us PWM period in microseconds.
 * @param[in] fade_duration_ms Duration of each fade phase in milliseconds.
 * @param[in] pause_duration_ms Pause between fade cycles in milliseconds.
 *
 * @return true if initialization was successful, otherwise false.
 */
bool led_controller_init(
    led_controller_t *controller,
    uint32_t pwm_period_us,
    uint32_t fade_duration_ms,
    uint32_t pause_duration_ms);

/**
 * @brief Prepares a new LED sequence.
 *
 * Selects the LED, resets the cycle counter and sets the controller to IDLE.
 * The sequence is not started until led_controller_resume() is called.
 *
 * @param[in,out] controller LED controller instance.
 * @param[in] led_idx LED index.
 * @param[in] cycle_count Number of complete fade-in/fade-out cycles.
 */
void led_controller_prepare(
    led_controller_t *controller,
    uint32_t led_idx,
    uint32_t cycle_count);

/**
 * @brief Pauses the currently running sequence.
 *
 * If a fade phase is active, its progress is preserved.
 * If the controller is in the inter-cycle pause phase, that pause is
 * suspended and will restart from the beginning when resumed.
 *
 * Calls made when the controller is IDLE or already PAUSED have no effect.
 *
 * @param[in,out] controller LED controller instance.
 */
void led_controller_pause(led_controller_t *controller);

/**
 * @brief Starts or resumes the prepared LED sequence.
 *
 * In IDLE, starts the sequence from the first fade-in phase.
 * In PAUSED, continues from the saved phase and progress.
 * In RUNNING, has no effect.
 *
 * @param[in,out] controller LED controller instance.
 */
void led_controller_resume(led_controller_t *controller);

/**
 * @brief Processes the current LED sequence.
 *
 * Advances the active fade or inter-cycle pause phase.
 * Software PWM is processed regardless of the controller state so that
 * the current brightness is maintained while the sequence is paused.
 *
 * When all requested cycles are complete, the controller returns to IDLE.
 *
 * @param[in,out] controller LED controller instance.
 *
 * @return true once when the sequence completes, otherwise false.
 */
bool led_controller_process(led_controller_t *controller);

/**
 * @brief Checks whether the controller is running.
 *
 * @param[in] controller LED controller instance.
 *
 * @return true if the controller is in RUNNING state, otherwise false.
 */
bool led_controller_is_running(const led_controller_t *controller);

#endif