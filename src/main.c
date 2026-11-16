#include <stdbool.h>
#include <stdint.h>

#include "nrfx_systick.h"

#include "btn/btn.h"
#include "main_behavior.h"

int main(void) {
    bool single_press_pending = false;
    uint16_t single_press_timeout_ms = 0;

    nrfx_systick_init();

    // btn_init() initializes LFCLK and the shared app_timer framework.
    btn_init();

    if (!main_behavior_init())
        while (1) nrfx_systick_delay_ms(1000);

    while (1) {
        btn_event_t event;

        // A double click also produces SINGLE_PRESS events. Delay the single
        // action until the double-press window has expired.
        while ((event = btn_event_take()) != BTN_EVENT_NONE) {
            if (event == BTN_EVENT_SINGLE_PRESS) {
                single_press_pending = true;
                // Must exceed BTN_DOUBLE_PRESS_TIME_MS in btn.c.
                single_press_timeout_ms = 1100;
            } else if (event == BTN_EVENT_DOUBLE_PRESS) {
                single_press_pending = false;
                single_press_timeout_ms = 0;
            }
        }

        if (single_press_pending) {
            if (single_press_timeout_ms <= 10) {
                single_press_pending = false;

                if (!main_behavior_toggle_yellow())
                    while (1) nrfx_systick_delay_ms(1000);
            } else
                single_press_timeout_ms -= 10;
        }

        nrfx_systick_delay_ms(10);
    }
}
