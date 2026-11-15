#include <stddef.h>
#include <stdint.h>

#include "nrf_assert.h"
#include "nrfx_pwm.h"

#include "pwm.h"

#define PWM_TOP_VALUE_MIN 3
#define PWM_TOP_VALUE_MAX 32767
#define PWM_IRQ_PRIORITY 6

static nrfx_pwm_t m_pwm = NRFX_PWM_INSTANCE(0);

static pwm_config_t m_config;
static nrf_pwm_values_individual_t m_sequence;
static nrf_pwm_sequence_t const m_pwm_sequence = {
    .values.p_individual = &m_sequence,
    .length = PWM_CHANNEL_COUNT,
    .repeats = 0,
    .end_delay = 0
};

static volatile uint8_t m_duty[PWM_CHANNEL_COUNT];
static uint16_t m_top_value;

static uint16_t pwm_top_value_calculate(uint32_t frequency_hz, nrf_pwm_clk_t *base_clock);
static uint16_t pwm_channel_value(pwm_channel_t channel);
static void pwm_sequence_update(void);
static void pwm_event_handler(nrfx_pwm_evt_type_t event);

void pwm_init(pwm_config_t const *config) {
    nrf_pwm_clk_t base_clock;
    nrfx_pwm_config_t nrfx_config = {
        .irq_priority = PWM_IRQ_PRIORITY,
        .count_mode = NRF_PWM_MODE_UP,
        .load_mode = NRF_PWM_LOAD_INDIVIDUAL,
        .step_mode = NRF_PWM_STEP_AUTO
    };

    ASSERT(config != NULL);

    // Keep our own copy; the caller may pass a temporary configuration.
    m_config = *config;

    m_top_value = pwm_top_value_calculate(m_config.frequency_hz, &base_clock);
    nrfx_config.base_clock = base_clock;
    nrfx_config.top_value = m_top_value;

    for (uint8_t i = 0; i < PWM_CHANNEL_COUNT; i++) {
        ASSERT(m_config.channels[i].pin < 48);
        ASSERT(m_config.channels[i].polarity == PWM_POLARITY_ACTIVE_HIGH ||
               m_config.channels[i].polarity == PWM_POLARITY_ACTIVE_LOW);

        m_duty[i] = 0;
        nrfx_config.output_pins[i] = m_config.channels[i].pin;

        if (m_config.channels[i].polarity == PWM_POLARITY_ACTIVE_LOW)
            nrfx_config.output_pins[i] |= NRFX_PWM_PIN_INVERTED;
    }

    pwm_sequence_update();

    ASSERT(nrfx_pwm_init(&m_pwm, &nrfx_config, pwm_event_handler) == NRFX_SUCCESS);

    // simple_playback uses the same sequence for SEQ0 and SEQ1.
    // Handle both end events as required by the driver API.
    (void)nrfx_pwm_simple_playback(
        &m_pwm,
        &m_pwm_sequence,
        1,
        NRFX_PWM_FLAG_LOOP |
        NRFX_PWM_FLAG_SIGNAL_END_SEQ0 |
        NRFX_PWM_FLAG_SIGNAL_END_SEQ1 |
        NRFX_PWM_FLAG_NO_EVT_FINISHED);
}

void pwm_set(pwm_channel_t channel, uint8_t duty) {
    ASSERT((uint32_t)channel < PWM_CHANNEL_COUNT);

    m_duty[channel] = duty;
}

static uint16_t pwm_top_value_calculate(uint32_t frequency_hz, nrf_pwm_clk_t *base_clock) {
    static const uint32_t base_clock_hz[] = {
        16000000,
        8000000,
        4000000,
        2000000,
        1000000,
        500000,
        250000,
        125000
    };

    static const nrf_pwm_clk_t base_clock_values[] = {
        NRF_PWM_CLK_16MHz,
        NRF_PWM_CLK_8MHz,
        NRF_PWM_CLK_4MHz,
        NRF_PWM_CLK_2MHz,
        NRF_PWM_CLK_1MHz,
        NRF_PWM_CLK_500kHz,
        NRF_PWM_CLK_250kHz,
        NRF_PWM_CLK_125kHz
    };

    // TOP_VALUE is limited to 3..32767. Select the fastest clock that fits.
    ASSERT(frequency_hz >= 4);
    ASSERT(frequency_hz <= 5333333);

    for (uint8_t i = 0; i < 8; i++) {
        uint32_t top_value = (base_clock_hz[i] + frequency_hz / 2) / frequency_hz;

        if (top_value < PWM_TOP_VALUE_MIN || top_value > PWM_TOP_VALUE_MAX)
            continue;

        *base_clock = base_clock_values[i];
        return (uint16_t)top_value;
    }

    ASSERT(0);
    *base_clock = NRF_PWM_CLK_125kHz;
    return PWM_TOP_VALUE_MIN;
}

static uint16_t pwm_channel_value(pwm_channel_t channel) {
    uint32_t active_ticks = ((uint32_t)m_duty[channel] * m_top_value + 127) / 255;
    uint16_t value = m_top_value - active_ticks;

    if (m_config.channels[channel].polarity == PWM_POLARITY_ACTIVE_LOW)
        value |= 0x8000;

    return value;
}

static void pwm_sequence_update(void) {
    m_sequence.channel_0 = pwm_channel_value(PWM_CHANNEL_0);
    m_sequence.channel_1 = pwm_channel_value(PWM_CHANNEL_1);
    m_sequence.channel_2 = pwm_channel_value(PWM_CHANNEL_2);
    m_sequence.channel_3 = pwm_channel_value(PWM_CHANNEL_3);
}

static void pwm_event_handler(nrfx_pwm_evt_type_t event) {
    if (event == NRFX_PWM_EVT_END_SEQ0 || event == NRFX_PWM_EVT_END_SEQ1)
        pwm_sequence_update();
}
