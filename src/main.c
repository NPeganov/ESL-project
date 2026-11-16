#include <stdint.h>

#include "nrfx_systick.h"

#include "leds/leds_pwm.h"

int main(void) {
    nrfx_systick_init();

    if (!leds_pwm_init())
        while (1) nrfx_systick_delay_ms(1000);

    while (1) {
        leds_pwm_set_yellow_brightness(100);
        nrfx_systick_delay_ms(300);
        leds_pwm_set_yellow_brightness(0);
        nrfx_systick_delay_ms(100);

        leds_pwm_set_rgb(255, 0, 0);
        nrfx_systick_delay_ms(300);
        leds_pwm_set_rgb(0, 255, 0);
        nrfx_systick_delay_ms(300);
        leds_pwm_set_rgb(0, 0, 255);
        nrfx_systick_delay_ms(300);
        leds_pwm_set_rgb(255, 255, 255);
        nrfx_systick_delay_ms(300);

        leds_pwm_set_hsv(0, 100, 100);
        nrfx_systick_delay_ms(300);
        leds_pwm_set_hsv(60, 100, 100);
        nrfx_systick_delay_ms(300);
        leds_pwm_set_hsv(120, 100, 100);
        nrfx_systick_delay_ms(300);
        leds_pwm_set_hsv(180, 100, 100);
        nrfx_systick_delay_ms(300);
        leds_pwm_set_hsv(240, 100, 100);
        nrfx_systick_delay_ms(300);
        leds_pwm_set_hsv(300, 100, 100);
        nrfx_systick_delay_ms(300);

        leds_pwm_set_hsv(0, 0, 100);
        nrfx_systick_delay_ms(300);
        leds_pwm_set_hsv(0, 100, 0);
        nrfx_systick_delay_ms(300);
    }
}
