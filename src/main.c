#include <stdbool.h>
#include <stdint.h>

#include "nrfx_systick.h"

#include "btn/btn.h"
#include "main_behavior.h"
#include "timer_service/timer_service.h"

static volatile bool m_single_press_pending;
static volatile uint16_t m_single_press_timeout_ms;

static void button_event_handler(btn_event_t event);

int main(void) {
    nrfx_systick_init();

    if (!timer_service_init())
        while (1) nrfx_systick_delay_ms(1000);

    if (!main_behavior_init())
        while (1) nrfx_systick_delay_ms(1000);

    btn_init(button_event_handler);
    btn_start();

    while (1) {
        // Delay the single action so a following double press can cancel it.
        if (m_single_press_pending) {
            if (m_single_press_timeout_ms <= 10) {
                m_single_press_pending = false;

                if (!main_behavior_toggle_yellow())
                    while (1) nrfx_systick_delay_ms(1000);
            } else
                m_single_press_timeout_ms -= 10;
        }

        nrfx_systick_delay_ms(10);
    }
}

static void button_event_handler(btn_event_t event) {
    if (event == BTN_EVENT_SINGLE_PRESS) {
        m_single_press_pending = true;
        m_single_press_timeout_ms = 1100;
    } else if (event == BTN_EVENT_DOUBLE_PRESS) {
        m_single_press_pending = false;
        m_single_press_timeout_ms = 0;
    }
}
