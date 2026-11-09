/** @file
 *
 * @defgroup blinky_example_main main.c
 * @{
 * @ingroup blinky_example
 * @brief Blinky Example Application main file.
 *
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"
#include "nrfx_systick.h"

#include "btn/btn.h"
#include "leds/leds.h"
#include "leds/led_controller.h"

#define PWM_PERIOD_US       1000
#define FADE_DURATION_MS    800
#define CYCLE_PAUSE_MS      500

/**
 * @brief Returns the number of blinks configured for an LED.
 *
 * @param[in] dev_id Device identifier.
 * @param[in] led_idx LED index.
 *
 * @return Number of blinks, or zero if the LED is not configured.
 */
static uint8_t get_led_blink_count(const char *dev_id, uint32_t led_idx)
{
    if (led_idx >= LEDS_NUMBER || led_idx >= sizeof("6609") - 1)
        return 0;

    if (!isdigit((unsigned char)dev_id[led_idx]))
        return 0;

    return (uint8_t)(dev_id[led_idx] - '0');
}

/**
 * @brief Finds the next LED configured for blinking.
 *
 * @param[in] dev_id Device identifier.
 * @param[in] current_idx Current LED index.
 *
 * @return Index of the next configured LED.
 */
static uint32_t get_next_led_index(const char *dev_id, uint32_t current_idx)
{
    for (uint32_t i = 1; i <= LEDS_NUMBER; i++)
    {
        uint32_t led_idx = (current_idx + i) % LEDS_NUMBER;

        if (get_led_blink_count(dev_id, led_idx) > 0)
            return led_idx;
    }

    return current_idx;
}

/**
 * @brief Function for application main entry.
 */
int main(void)
{
    const char dev_id[] = "6609";

    led_controller_t controller;

    uint32_t led_idx = 0;
    bool sequence_started = false;
    bool controller_paused = false;

    leds_init();
    leds_off();

    nrfx_systick_init();

    btn_init();

    if (!led_controller_init(
            &controller,
            PWM_PERIOD_US,
            FADE_DURATION_MS,
            CYCLE_PAUSE_MS)) while (true);

    led_idx = get_next_led_index(dev_id, LEDS_NUMBER - 1);

    while (true)
    {
        btn_event_t event;

        // Handle double-press events.
        while ((event = btn_event_take()) != BTN_EVENT_NONE)
        {
            if (event != BTN_EVENT_DOUBLE_PRESS) continue;

            if (!sequence_started)
            {
                led_controller_start(
                    &controller,
                    led_idx,
                    get_led_blink_count(dev_id, led_idx));

                sequence_started = true;
            }
            else if (controller_paused)
            {
                led_controller_resume(&controller);
                controller_paused = false;
            }
            else
            {
                led_controller_pause(&controller);
                controller_paused = true;
            }
        }

        // Update the current LED sequence.
        led_controller_process(&controller);

        // Start the next LED when the current sequence is complete.
        if (sequence_started &&
            !controller_paused &&
            !led_controller_is_active(&controller))
        {
            led_idx = get_next_led_index(dev_id, led_idx);

            led_controller_start(
                &controller,
                led_idx,
                get_led_blink_count(dev_id, led_idx));
        }
    }
}