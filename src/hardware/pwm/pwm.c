#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nrf_assert.h"
#include "nrf_gpio.h"
#include "nrfx_pwm.h"

#include "pwm.h"
#include "pwm_internal.h"

// PWM interrupt priority, matching the other application peripherals.
#define PWM_IRQ_PRIORITY 6

// Target interval for refreshing the output sequence.
#define PWM_UPDATE_INTERVAL_MS 10
#define PWM_MILLISECONDS_PER_SECOND 1000

static nrfx_pwm_t m_pwm = NRFX_PWM_INSTANCE(0); // Hardware PWM instance.
static bool m_channel_active_low[PWM_CHANNEL_COUNT]; // Active level configured for each channel.
static nrf_pwm_values_individual_t m_sequence; // Persistent buffer read by the PWM peripheral.
static volatile uint8_t m_duty[PWM_CHANNEL_COUNT]; // Current duty values set by pwm_set().
static uint16_t m_top_value; // Selected period count, also used by the event handler.
static uint32_t m_update_interval; // Number of PWM periods between sequence updates.
static uint32_t m_update_counter; // PWM periods elapsed since the last sequence update.

// Configure the output pins and retain each channel's active-level setting.
static void pwm_channels_configure(
    pwm_config_t const *config,
    nrfx_pwm_config_t *nrfx_config);

// Initialize the nrfx driver and start continuous playback of the sequence buffer.
static bool pwm_driver_start(nrfx_pwm_config_t const *config);

// Handle sequence-end events and refresh the buffer at the configured interval.
static void pwm_event_handler(nrfx_pwm_evt_type_t event);

bool pwm_init(pwm_config_t const *config) {
    pwm_timing_t timing;
    nrfx_pwm_config_t nrfx_config;

    ASSERT(config != NULL);

    // Configure the PWM counter and sequence loading behavior.
    nrfx_config.irq_priority = PWM_IRQ_PRIORITY;
    nrfx_config.count_mode = NRF_PWM_MODE_UP;
    nrfx_config.load_mode = NRF_PWM_LOAD_INDIVIDUAL;
    nrfx_config.step_mode = NRF_PWM_STEP_AUTO;

    // Select timing settings supported by the hardware.
    if (!pwm_timing_select(config->frequency_hz, &timing))
        return false;

    // Apply the selected clock and period to the nrfx configuration.
    // Keep the period count for sequence updates from the event handler.
    m_top_value = timing.top_value;
    nrfx_config.base_clock = timing.base_clock;
    nrfx_config.top_value = timing.top_value;

    // Configure output pins and initialize each channel's runtime state.
    pwm_channels_configure(config, &nrfx_config);

    // Convert the refresh interval from milliseconds to PWM periods.
    // Use the actual frequency because the selected timing may round the request.
    m_update_interval = (
        timing.actual_frequency_hz * PWM_UPDATE_INTERVAL_MS +
        PWM_MILLISECONDS_PER_SECOND / 2) / PWM_MILLISECONDS_PER_SECOND;

    // Ensure at least one complete PWM period passes between buffer updates.
    if (m_update_interval == 0)
        m_update_interval = 1;

    // Prepare the initial sequence before starting hardware playback.
    pwm_sequence_update(&m_sequence, m_duty, m_top_value, m_channel_active_low);

    return pwm_driver_start(&nrfx_config);
}

void pwm_set(pwm_channel_t channel, uint8_t duty) {
    ASSERT((uint32_t)channel < PWM_CHANNEL_COUNT);

    // The event handler copies the latest duty values into the playback buffer.
    m_duty[channel] = duty;
}

static void pwm_channels_configure(
    pwm_config_t const *config,
    nrfx_pwm_config_t *nrfx_config) {

    for (uint8_t i = 0; i < PWM_CHANNEL_COUNT; i++) {
        // Valid pins and polarity values are configuration preconditions.
        ASSERT(nrf_gpio_pin_present_check(config->channels[i].pin));
        ASSERT(config->channels[i].polarity == PWM_POLARITY_ACTIVE_HIGH ||
               config->channels[i].polarity == PWM_POLARITY_ACTIVE_LOW);

        // Start each channel inactive and retain its logical active level.
        m_duty[i] = 0;
        m_channel_active_low[i] =
            config->channels[i].polarity == PWM_POLARITY_ACTIVE_LOW;

        // Connect the configured GPIO pin to the corresponding PWM output.
        nrfx_config->output_pins[i] = config->channels[i].pin;

        if (m_channel_active_low[i]) {
            // Keep the GPIO idle level high while the PWM peripheral is inactive.
            nrfx_config->output_pins[i] |= NRFX_PWM_PIN_INVERTED;
        }
    }
}

static bool pwm_driver_start(nrfx_pwm_config_t const *config) {
    nrf_pwm_sequence_t sequence;

    // Initialize the nrfx driver before configuring and starting playback.
    if (nrfx_pwm_init(&m_pwm, config, pwm_event_handler) != NRFX_SUCCESS)
        return false;

    sequence.values.p_individual = &m_sequence;

    // Individual load mode uses four 16-bit values, one for each PWM channel.
    sequence.length = PWM_CHANNEL_COUNT;
    sequence.repeats = 0;
    sequence.end_delay = 0;

    // simple_playback starts continuous playback of the sequence buffer.
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
    // Only sequence-end events indicate that the playback buffer can be updated.
    if (event != NRFX_PWM_EVT_END_SEQ0 && event != NRFX_PWM_EVT_END_SEQ1)
        return;

    // Wait until the configured number of PWM periods has elapsed.
    if (++m_update_counter < m_update_interval)
        return;

    // Restart the interval and refresh the sequence from the latest channel settings.
    m_update_counter = 0;
    pwm_sequence_update(&m_sequence, m_duty, m_top_value, m_channel_active_low);
}
