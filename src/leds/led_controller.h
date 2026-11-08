#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

#include "nrfx_systick.h"
#include "fade.h"
#include "soft_pwm.h"

typedef struct
{
    soft_pwm_t pwm;                     // Software PWM instance.
    fade_t fade;                        // Fade instance.
    nrfx_systick_state_t pause_start;   // Start time of the current pause.
    uint32_t fade_duration_ms;          // Fade duration in milliseconds.
    uint32_t pause_duration_ms;         // Pause duration between cycles in milliseconds.
    uint32_t pause_elapsed_ms;          // Elapsed pause time in milliseconds.
    uint32_t cycle_count;               // Number of complete fade cycles.
    uint32_t cycles_completed;          // Number of completed fade cycles.
    uint32_t led_idx;                   // LED index controlled by this controller.
    uint8_t phase;                      // Current sequence phase.
    bool initialized;                   // True after successful initialization.
    bool active;                        // True while the sequence is running or paused.
    bool paused;                        // True while the sequence is paused.
} led_controller_t;

/**
 * @brief Initialize a LED controller instance.
 *
 * @param controller LED controller instance.
 * @param pwm_period_us PWM period in microseconds.
 * @param fade_duration_ms Fade duration in milliseconds.
 * @param pause_duration_ms Pause duration between cycles in milliseconds.
 *
 * @return true if initialization was successful, otherwise false.
 */
bool led_controller_init(
    led_controller_t *controller,
    uint32_t pwm_period_us,
    uint32_t fade_duration_ms,
    uint32_t pause_duration_ms);

/**
 * @brief Start a LED sequence.
 *
 * @param controller LED controller instance.
 * @param led_idx LED index.
 * @param cycle_count Number of complete fade-in/fade-out cycles.
 */
void led_controller_start(
    led_controller_t *controller,
    uint32_t led_idx,
    uint32_t cycle_count);

/**
 * @brief Pause the active LED sequence.
 *
 * @param controller LED controller instance.
 */
void led_controller_pause(led_controller_t *controller);

/**
 * @brief Resume a paused LED sequence.
 *
 * @param controller LED controller instance.
 */
void led_controller_resume(led_controller_t *controller);

/**
 * @brief Process the LED controller state.
 *
 * @param controller LED controller instance.
 */
void led_controller_process(led_controller_t *controller);

/**
 * @brief Check whether the LED sequence is active.
 *
 * @param controller LED controller instance.
 *
 * @return true if the LED sequence is active, otherwise false.
 */
bool led_controller_is_active(const led_controller_t *controller);

#endif /* LED_CONTROLLER_H */