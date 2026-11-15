#include "nrfx_systick.h"

#include "leds/leds.h"
#include "pwm/pwm.h"

int main(void) {
    pwm_config_t pwm_config = {
        .frequency_hz = 1000,
        .channels = {
            { .pin = LED_1, .polarity = PWM_POLARITY_ACTIVE_LOW },
            { .pin = LED_2, .polarity = PWM_POLARITY_ACTIVE_LOW },
            { .pin = LED_3, .polarity = PWM_POLARITY_ACTIVE_LOW },
            { .pin = LED_4, .polarity = PWM_POLARITY_ACTIVE_LOW }
        }
    };

    leds_init();
    nrfx_systick_init();
    pwm_init(&pwm_config);

    while (1) {
        // Fade LED0 up and down to verify hardware PWM.
        for (uint16_t duty = 0; duty <= 255; duty++) {
            pwm_set(PWM_CHANNEL_0, (uint8_t)duty);
            nrfx_systick_delay_ms(20);
        }

        for (int16_t duty = 254; duty >= 0; duty--) {
            pwm_set(PWM_CHANNEL_0, (uint8_t)duty);
            nrfx_systick_delay_ms(20);
        }
    }
}
