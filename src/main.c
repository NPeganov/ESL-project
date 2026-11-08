/** @file
 *
 * @defgroup blinky_example_main main.c
 * @{
 * @ingroup blinky_example
 * @brief Blinky Example Application main file.
 *
 * This file contains the source code for a sample application to blink LEDs.
 *
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#include "nrfx_systick.h"

#include "btn/btn.h"
#include "leds/leds.h"
#include "leds/led_controller.h"

/**
 * @brief Function for application main entry.
 */

#define PWM_PERIOD_US       1000
#define FADE_DURATION_MS    800
#define CYCLE_PAUSE_MS      500

int main(void)
{
    const char dev_id[] = "6609";

    led_controller_t controller;
    bool button_pressed = false;

    leds_init();
    leds_off();

    nrfx_systick_init();
    btn_init();

    if (!led_controller_init(
            &controller,
            PWM_PERIOD_US,
            FADE_DURATION_MS,
            CYCLE_PAUSE_MS)) while (true); // incorrect initialization

    while (true)
    {
        for (uint32_t i = 0; i < LEDS_NUMBER && i < sizeof(dev_id) - 1; i++)
        {
            if (!isdigit((unsigned char)dev_id[i])) continue;

            uint8_t blink_remaining = (uint8_t)(dev_id[i] - '0');
            if (blink_remaining == 0) continue;

            // Wait until the button is pressed.
            while (!button_pressed)
            {
                btn_event_t event = btn_event_take();

                if (event == BTN_EVENT_PRESS)
                    button_pressed = true;
                else if (event == BTN_EVENT_RELEASE)
                    button_pressed = false;

                led_controller_process(&controller);
            }

            led_controller_start(
                &controller,
                i,
                blink_remaining);

            while (led_controller_is_active(&controller))
            {
                btn_event_t event;

                while ((event = btn_event_take()) != BTN_EVENT_NONE)
                {
                    if (event == BTN_EVENT_PRESS)
                    {
                        button_pressed = true;
                        led_controller_resume(&controller);
                    }
                    else if (event == BTN_EVENT_RELEASE)
                    {
                        button_pressed = false;
                        led_controller_pause(&controller);
                    }
                }

                led_controller_process(&controller);
            }
        }
    }
}