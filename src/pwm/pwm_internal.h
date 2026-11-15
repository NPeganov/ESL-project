#ifndef _PWM_INTERNAL_H_
#define _PWM_INTERNAL_H_

#include "pwm.h"

bool pwm_internal_init(pwm_config_t const *config);
void pwm_internal_set(pwm_channel_t channel, uint8_t duty);

#endif
