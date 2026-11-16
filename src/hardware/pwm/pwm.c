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
static nrf_pwm_values_individual_t m_sequence_values;
static volatile uint8_t m_duty[PWM_CHANNEL_COUNT];
static uint16_t m_top_value;
static uint32_t m_update_periods;

// Configure channel pins, polarity, and initial duty values.
static void pwm_channels_configure(
    pwm_config_t const *config,
    nrfx_pwm_config_t *nrfx_config);

// Initialize the nrfx driver and start looping sequence playback.
static bool pwm_driver_start(nrfx_pwm_config_t const *config);

// Handle sequence-end events and refresh playback values periodically.
static void pwm_playback_event_handler(nrfx_pwm_evt_type_t event);

// Select hardware timing, configure all channels, then start PWM playback.
bool pwm_init(pwm_config_t const *config) {
    pwm_timing_t timing;
    nrfx_pwm_config_t nrfx_config;

    ASSERT(config != NULL);

    // Configure the common operating mode used by all four channels.
    nrfx_config.irq_priority = PWM_IRQ_PRIORITY;
    nrfx_config.count_mode = NRF_PWM_MODE_UP;
    nrfx_config.load_mode = NRF_PWM_LOAD_INDIVIDUAL;
    nrfx_config.step_mode = NRF_PWM_STEP_AUTO;

    // Reject frequencies that cannot be represented by the PWM hardware.
    if (!pwm_timing_select(config->frequency_hz, &timing))
        return false;

    m_top_value = timing.top_value;
    nrfx_config.base_clock = timing.base_clock;
    nrfx_config.top_value = timing.top_value;

    // Copy pin and polarity settings into the driver's configuration.
    pwm_channels_configure(config, &nrfx_config);

    // Convert the refresh interval to a number of PWM periods.
    // Use the rounded hardware frequency to account for timing quantization.
    m_update_periods = (
        timing.actual_frequency_hz * PWM_UPDATE_INTERVAL_MS +
        PWM_MILLISECONDS_PER_SECOND / 2) / PWM_MILLISECONDS_PER_SECOND;

    if (m_update_periods == 0)
        m_update_periods = 1;

    // Prepare the initial all-zero sequence before starting the peripheral.
    pwm_sequence_update(
        &m_sequence_values, m_duty, m_top_value, m_channel_active_low);

    return pwm_driver_start(&nrfx_config);
}

void pwm_set(pwm_channel_t channel, uint8_t duty) {
    ASSERT((uint32_t)channel < PWM_CHANNEL_COUNT);

    // The playback handler copies the latest duty values into the sequence.
    m_duty[channel] = duty;
}

static void pwm_channels_configure(
    pwm_config_t const *config,
    nrfx_pwm_config_t *nrfx_config) {

    for (uint8_t i = 0; i < PWM_CHANNEL_COUNT; i++) {
        ASSERT(nrf_gpio_pin_present_check(config->channels[i].pin));
        ASSERT(config->channels[i].polarity == PWM_POLARITY_ACTIVE_HIGH ||
               config->channels[i].polarity == PWM_POLARITY_ACTIVE_LOW);

        // Keep the logical polarity for compare-value calculation.
        m_duty[i] = 0;
        m_channel_active_low[i] =
            config->channels[i].polarity == PWM_POLARITY_ACTIVE_LOW;

        nrfx_config->output_pins[i] = config->channels[i].pin;

        // Configure the peripheral output inversion for the physical pin.
        if (m_channel_active_low[i])
            nrfx_config->output_pins[i] |= NRFX_PWM_PIN_INVERTED;
    }
}

static bool pwm_driver_start(nrfx_pwm_config_t const *config) {
    nrf_pwm_sequence_t sequence;

    // The driver owns the peripheral setup; do not start playback on failure.
    if (nrfx_pwm_init(&m_pwm, config, pwm_playback_event_handler) != NRFX_SUCCESS)
        return false;

    // The sequence values are updated in place by the playback handler.
    sequence.values.p_individual = &m_sequence_values;
    sequence.length = PWM_CHANNEL_COUNT;
    sequence.repeats = 0;
    sequence.end_delay = 0;

    // Loop continuously and request callbacks at sequence boundaries.
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

static void pwm_playback_event_handler(nrfx_pwm_evt_type_t event) {
    static uint32_t periods_since_update;

    // Ignore events unrelated to the looping sequence boundaries.
    if (event != NRFX_PWM_EVT_END_SEQ0 && event != NRFX_PWM_EVT_END_SEQ1)
        return;

    // Refresh the sequence only after the configured number of periods.
    if (++periods_since_update < m_update_periods)
        return;

    periods_since_update = 0;
    pwm_sequence_update(
        &m_sequence_values, m_duty, m_top_value, m_channel_active_low);
}
