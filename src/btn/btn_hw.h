#ifndef _BTN_HW_H_
#define _BTN_HW_H_

#include <stdbool.h>

// Callback for confirmed button state changes.
typedef void (*btn_hw_callback_t)(bool pressed);

// Configures GPIOTE and creates the debounce timer.
// LFCLK and app_timer must already be initialized.
void btn_hw_init(btn_hw_callback_t callback);

// Enables GPIO events and starts debounce of the initial button state.
// The callback is invoked after the initial state has been confirmed.
void btn_hw_start(void);

#endif