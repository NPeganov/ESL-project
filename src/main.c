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

#include "nrf_delay.h"
#include "nrfx_systick.h"

#include "lib/leds.h"
#include "lib/btn.h"
#include "lib/soft_pwm.h"
#include "lib/fade.h"

/**
 * @brief Function for application main entry.
 */

#define PWM_PERIOD_US       1000U
#define FADE_DURATION_MS    800U

static uint32_t pwm_led_idx = 0U;

static void pwm_led_output(bool state, void *context)
{
    uint32_t led_idx = *(uint32_t *)context;

    if (state) led_on(led_idx);
    else led_off(led_idx);
}

int main(void)
{
    const char dev_id[] = "6609";

    soft_pwm_t pwm;
    fade_t fade;

    leds_init();
    leds_off();
    btn_init();

    nrfx_systick_init();

    if (!soft_pwm_init(
            &pwm,
            PWM_PERIOD_US,
            pwm_led_output,
            &pwm_led_idx)) while (true); // incorrect initialization
    
    fade_init(&fade, &pwm);

    while (true)
    {
        for (uint8_t i = 0; i < LEDS_NUMBER; i++)
        {
            if (!isdigit((unsigned char)dev_id[i])) continue;

            uint8_t blink_remaining = (uint8_t)(dev_id[i] - '0');
            if (blink_remaining == 0) continue;

            pwm_led_idx = i;

            for (uint8_t j = 0; j < blink_remaining; j++)
            {
                bool fade_in = true;
                bool fade_started = false;

                while (true)
                {
                    soft_pwm_process(&pwm);

                    // Pause the fade while the button is released.
                    if (!btn_pressed())
                    {
                        if (fade_is_active(&fade)) fade_stop(&fade);

                        continue;
                    }

                    if (!fade_started)
                    {
                        fade_start(&fade, 0, FADE_PERCENT_MAX, FADE_DURATION_MS);
                        fade_started = true;
                    }
                    else if (fade_is_paused(&fade))
                    {
                        fade_resume(&fade);
                    }

                    fade_process(&fade);

                    if (!fade_is_active(&fade) && !fade_is_paused(&fade))
                    {
                        if (fade_in)
                        {
                            fade_in = false;

                            fade_start(
                                &fade,
                                FADE_PERCENT_MAX,
                                0,
                                FADE_DURATION_MS);
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }
        }
    }
}

/**
 *@}
 **/