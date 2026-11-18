#include <stdbool.h>

#include "app_error.h"
#include "nrf.h"
#include "nrf_delay.h"

#include "leds/leds_pwm.h"

int main(void) {
    if (!leds_pwm_init())
        APP_ERROR_HANDLER(NRF_ERROR_INTERNAL);

    while (true) {
        // Warm orange.
        leds_pwm_set_rgb(255, 80, 0);
        leds_pwm_set_yellow_brightness(20);
        nrf_delay_ms(300);
        leds_pwm_set_yellow_brightness(80);
        nrf_delay_ms(300);

        // Turquoise.
        leds_pwm_set_rgb(0, 180, 160);
        leds_pwm_set_yellow_brightness(20);
        nrf_delay_ms(300);
        leds_pwm_set_yellow_brightness(80);
        nrf_delay_ms(300);

        // Violet.
        leds_pwm_set_rgb(160, 0, 255);
        leds_pwm_set_yellow_brightness(20);
        nrf_delay_ms(300);
        leds_pwm_set_yellow_brightness(80);
        nrf_delay_ms(300);
    }
}
