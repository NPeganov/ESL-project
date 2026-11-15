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
    pwm_init(&pwm_config);

    while (1) {}
}
