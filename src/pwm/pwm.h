#ifndef _PWM_H_
#define _PWM_H_

#include <stdbool.h>
#include <stdint.h>

#define PWM_CHANNEL_COUNT 4

typedef enum
{
    PWM_CHANNEL_0,
    PWM_CHANNEL_1,
    PWM_CHANNEL_2,
    PWM_CHANNEL_3
} pwm_channel_t;

typedef enum
{
    PWM_POLARITY_ACTIVE_HIGH,
    PWM_POLARITY_ACTIVE_LOW
} pwm_polarity_t;

typedef struct
{
    uint8_t pin;
    pwm_polarity_t polarity;
} pwm_channel_config_t;

typedef struct
{
    uint32_t frequency_hz;
    pwm_channel_config_t channels[PWM_CHANNEL_COUNT];
} pwm_config_t;

bool pwm_init(pwm_config_t const *config);
void pwm_set(pwm_channel_t channel, uint8_t duty);

#endif
