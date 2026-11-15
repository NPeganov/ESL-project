#ifndef _PWM_HW_H_
#define _PWM_HW_H_

#include "pwm.h"

bool pwm_hw_init(pwm_config_t const *config, volatile uint8_t const *duty);

#endif
