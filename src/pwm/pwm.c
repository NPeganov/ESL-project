#include <stdint.h>

#include "nrf_assert.h"

#include "pwm.h"
#include "pwm_hw.h"

// Duty values are written by pwm_set() and read by the PWM hardware module.
static volatile uint8_t m_duty[PWM_CHANNEL_COUNT];

bool pwm_init(pwm_config_t const *config) {
    ASSERT(config != NULL);

    // Initialize all channels with zero duty.
    for (uint8_t i = 0; i < PWM_CHANNEL_COUNT; i++)
        m_duty[i] = 0;

    return pwm_hw_init(config, m_duty);
}

void pwm_set(pwm_channel_t channel, uint8_t duty) {
    ASSERT((uint32_t)channel < PWM_CHANNEL_COUNT);

    m_duty[channel] = duty;
}
