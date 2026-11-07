#ifndef _FADE_H_
#define _FADE_H_

#include <stdbool.h>
#include <stdint.h>

#include "nrfx_systick.h"
#include "soft_pwm.h"

#define FADE_UPDATE_INTERVAL_MS    1U
#define FADE_PERCENT_MAX           100U

typedef struct
{
    soft_pwm_t *pwm;                    // Pointer to the software PWM instance.
    uint8_t start_duty;                 // Initial duty cycle in percent.
    uint8_t target_duty;                // Target duty cycle in percent.
    uint32_t duration_ms;               // Fade duration in milliseconds.
    uint32_t elapsed_ms;                // Elapsed fade time in milliseconds.
    nrfx_systick_state_t update_time;   // Start time of the current update interval.
    bool active;                        // True while the fade is active.
    bool paused;                        // True while the fade is paused.
} fade_t;

/**
 * @brief Initialize a fade instance.
 *
 * @param fade Fade instance to initialize.
 * @param pwm Software PWM instance used by the fade.
 */
void fade_init(
    fade_t *fade,
    soft_pwm_t *pwm);

/**
 * @brief Start a fade operation.
 *
 * @param fade Fade instance.
 * @param start_duty Initial duty cycle in percent.
 * @param target_duty Target duty cycle in percent.
 * @param duration_ms Fade duration in milliseconds.
 */
void fade_start(
    fade_t *fade,
    uint8_t start_duty,
    uint8_t target_duty,
    uint32_t duration_ms);

/**
 * @brief Stop the active fade operation.
 *
 * @param fade Fade instance.
 */
void fade_stop(fade_t *fade);

/**
 * @brief Resume a previously stopped fade operation.
 *
 * @param fade Fade instance.
 */
void fade_resume(fade_t *fade);

/**
 * @brief Process the active fade.
 *
 * @param fade Fade instance.
 */
void fade_process(fade_t *fade);

/**
 * @brief Check whether the fade is active.
 *
 * @param fade Fade instance.
 *
 * @return true if the fade is active, otherwise false.
 */
bool fade_is_active(const fade_t *fade);

/**
 * @brief Check whether the fade is paused.
 *
 * @param fade Fade instance.
 *
 * @return true if the fade is paused, otherwise false.
 */
bool fade_is_paused(const fade_t *fade);

#endif