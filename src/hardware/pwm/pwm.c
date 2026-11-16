#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nrf_assert.h"
#include "nrf_gpio.h"
#include "nrfx_pwm.h"

#include "pwm.h"
#include "internal/pwm_internal.h"

#define PWM_IRQ_PRIORITY 6
#define PWM_UPDATE_INTERVAL_MS 10
#define PWM_MILLISECONDS_PER_SECOND 1000

static nrfx_pwm_t m_pwm = NRFX_PWM_INSTANCE(0);
static bool m_channel_active_low[PWM_CHANNEL_COUNT];
static nrf_pwm_values_individual_t m_sequence;
static volatile uint8_t m_duty[PWM_CHANNEL_COUNT];
static uint16_t m_top_value;
static uint32_t m_update_interval;

// Configure channel pins and initial output states.
static void pwm_channels_configure(
    pwm_config_t const *config,
    nrfx_pwm_config_t *nrfx_config);

// Initialize the driver and start continuous sequence playback.
static bool pwm_driver_start(nrfx_pwm_config_t const *config);

// Refresh the sequence buffer after the configured number of PWM periods.
static void pwm_event_handler(nrfx_pwm_evt_type_t event);

bool pwm_init(pwm_config_t const *config) {
    pwm_timing_t timing;
    nrfx_pwm_config_t nrfx_config;

    ASSERT(config != NULL);

    nrfx_config.irq_priority = PWM_IRQ_PRIORITY;
    nrfx_config.count_mode = NRF_PWM_MODE_UP;
    nrfx_config.load_mode = NRF_PWM_LOAD_INDIVIDUAL;
    nrfx_config.step_mode = NRF_PWM_STEP_AUTO;

    if (!pwm_timing_select(config->frequency_hz, &timing))
        return false;

    m_top_value = timing.top_value;
    nrfx_config.base_clock = timing.base_clock;
    nrfx_config.top_value = timing.top_value;

    pwm_channels_configure(config, &nrfx_config);

    // Account for frequency rounding when setting the buffer refresh interval.
    m_update_interval = (
        timing.actual_frequency_hz * PWM_UPDATE_INTERVAL_MS +
        PWM_MILLISECONDS_PER_SECOND / 2) / PWM_MILLISECONDS_PER_SECOND;

    if (m_update_interval == 0)
        m_update_interval = 1;

    pwm_sequence_update(&m_sequence, m_duty, m_top_value, m_channel_active_low);

    return pwm_driver_start(&nrfx_config);
}

void pwm_set(pwm_channel_t channel, uint8_t duty) {
    ASSERT((uint32_t)channel < PWM_CHANNEL_COUNT);

    m_duty[channel] = duty;
}

static void pwm_channels_configure(
    pwm_config_t const *config,
    nrfx_pwm_config_t *nrfx_config) {

    for (uint8_t i = 0; i < PWM_CHANNEL_COUNT; i++) {
        ASSERT(nrf_gpio_pin_present_check(config->channels[i].pin));
        ASSERT(config->channels[i].polarity == PWM_POLARITY_ACTIVE_HIGH ||
               config->channels[i].polarity == PWM_POLARITY_ACTIVE_LOW);

        m_duty[i] = 0;
        m_channel_active_low[i] =
            config->channels[i].polarity == PWM_POLARITY_ACTIVE_LOW;

        nrfx_config->output_pins[i] = config->channels[i].pin;

        if (m_channel_active_low[i])
            nrfx_config->output_pins[i] |= NRFX_PWM_PIN_INVERTED;
    }
}

static bool pwm_driver_start(nrfx_pwm_config_t const *config) {
    nrf_pwm_sequence_t sequence;

    if (nrfx_pwm_init(&m_pwm, config, pwm_event_handler) != NRFX_SUCCESS)
        return false;

    sequence.values.p_individual = &m_sequence;
    sequence.length = PWM_CHANNEL_COUNT;
    sequence.repeats = 0;
    sequence.end_delay = 0;

    nrfx_pwm_simple_playback(
        &m_pwm,
        &sequence,
        1,
        NRFX_PWM_FLAG_LOOP |
        NRFX_PWM_FLAG_SIGNAL_END_SEQ0 |
        NRFX_PWM_FLAG_SIGNAL_END_SEQ1 |
        NRFX_PWM_FLAG_NO_EVT_FINISHED);

    return true;
}

static void pwm_event_handler(nrfx_pwm_evt_type_t event) {
    static uint32_t update_counter;

    if (event != NRFX_PWM_EVT_END_SEQ0 && event != NRFX_PWM_EVT_END_SEQ1)
        return;

    if (++update_counter < m_update_interval)
        return;

    update_counter = 0;
    pwm_sequence_update(&m_sequence, m_duty, m_top_value, m_channel_active_low);
}
