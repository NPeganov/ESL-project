#include "pwm.h"
#include "pwm_internal.h"

bool pwm_init(pwm_config_t const *config) {
    return pwm_internal_init(config);
}

void pwm_set(pwm_channel_t channel, uint8_t duty) {
    pwm_internal_set(channel, duty);
}
