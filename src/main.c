// temporary cleanup marker
#include <stdint.h>

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

    if (!pwm_init(&pwm_config))
        while (1) nrfx_systick_delay_ms(1000);

    while (1) {
        for (uint8_t i = 0; i < PWM_CHANNEL_COUNT; i++) {
            pwm_set((pwm_channel_t)i, 255);
            nrfx_systick_delay_ms(100);
            pwm_set((pwm_channel_t)i, 0);
        }
    }
}
