#ifndef _BTN_HW_H_
#define _BTN_HW_H_

#include <stdbool.h>

/**
 * @brief Callback for confirmed button state changes.
 *
 * @param[in] pressed True if the button is pressed.
 */
typedef void (*btn_hw_callback_t)(bool pressed);

/**
 * @brief Initializes button hardware handling.
 *
 * Initializes LFCLK, app_timer and GPIOTE.
 *
 * @param[in] callback Callback for confirmed button state changes.
 */
void btn_hw_init(btn_hw_callback_t callback);

/**
 * @brief Starts button hardware event handling.
 *
 * Enables GPIOTE events and establishes the initial button state.
 */
void btn_hw_start(void);

#endif