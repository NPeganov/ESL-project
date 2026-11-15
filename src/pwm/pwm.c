#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nrf_assert.h"
#include "nrf_gpio.h"
#include "nrfx_pwm.h"

#include "pwm.h"
#include "pwm_internal.h"

// Keep the PWM interrupt at the same priority as other application peripherals.
#define PWM_IRQ_PRIORITY 6

// Recalculate the output sequence every 10 ms.
#define PWM_UPDATE_INTERVAL_MS 10
#define PWM_MILLISECONDS_PER_SECOND 1000

// Hardware PWM instance used by this module.
static nrfx_pwm_t m_pwm = NRFX_PWM_INSTANCE(0);

// Per-channel polarity bits added to the PWM sequence values.
static uint16_t m_channel_polarity_mask[PWM_CHANNEL_COUNT];

// Buffer containing the compare values for all four PWM channels.
static nrf_pwm_values_individual_t m_sequence;

// Sequence descriptor passed to the nrfx PWM driver.
static nrf_pwm_sequence_t m_pwm_sequence;

// Duty values are written by pwm_set() and read by the PWM interrupt handler.
static volatile uint8_t m_duty[PWM_CHANNEL_COUNT];

// Selected PWM period count, shared by all channels.
static uint16_t m_top_value;

// Number of PWM periods between sequence updates.
static uint32_t m_update_interval;

// PWM periods elapsed since the last sequence update.
static uint32_t m_update_counter;

// Validate channel settings and prepare the nrfx output pin configuration.
static void pwm_channels_configure(
    pwm_config_t const *config,
    nrfx_pwm_config_t *nrfx_config);

// Initialize the nrfx driver and start continuous playback of the PWM sequence.
static bool pwm_driver_start(nrfx_pwm_config_t const *config);

// Handle sequence-end events and update the PWM buffer at the configured interval.
static void pwm_event_handler(nrfx_pwm_evt_type_t event);

bool pwm_init(pwm_config_t const *config) {
    pwm_timing_t timing;
    nrfx_pwm_config_t nrfx_config;

    ASSERT(config != NULL);

    // Configure the PWM counting and sequence loading modes.
    nrfx_config.irq_priority = PWM_IRQ_PRIORITY;
    nrfx_config.count_mode = NRF_PWM_MODE_UP;
    nrfx_config.load_mode = NRF_PWM_LOAD_INDIVIDUAL;
    nrfx_config.step_mode = NRF_PWM_STEP_AUTO;

    // Reject frequencies outside the range supported by the hardware.
    if (!pwm_timing_select(config->frequency_hz, &timing))
        return false;

    // Apply the selected timing to the driver configuration.
    m_top_value = timing.top_value;
    nrfx_config.base_clock = timing.base_clock;
    nrfx_config.top_value = timing.top_value;

    // Configure output pins and initialize each channel's runtime state.
    pwm_channels_configure(config, &nrfx_config);

    // Convert the update interval from milliseconds to PWM periods.
    // Use the actual frequency because the requested frequency may be rounded.
    m_update_interval = (
        timing.actual_frequency_hz * PWM_UPDATE_INTERVAL_MS +
        PWM_MILLISECONDS_PER_SECOND / 2) / PWM_MILLISECONDS_PER_SECOND;

    // Ensure the sequence is not updated more than once per PWM period.
    if (m_update_interval == 0)
        m_update_interval = 1;

    // Prepare the initial sequence before starting hardware playback.
    pwm_sequence_update(&m_sequence, m_duty, m_top_value, m_channel_polarity_mask);

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
        // Channel pins and polarity values are configuration preconditions.
        ASSERT(nrf_gpio_pin_present_check(config->channels[i].pin));
        ASSERT(config->channels[i].polarity == PWM_POLARITY_ACTIVE_HIGH ||
               config->channels[i].polarity == PWM_POLARITY_ACTIVE_LOW);

        // Start every channel with zero duty and no polarity bit.
        m_channel_polarity_mask[i] = 0;
        m_duty[i] = 0;

        // Assign the GPIO pin to the corresponding PWM output.
        nrfx_config->output_pins[i] = config->channels[i].pin;

        if (config->channels[i].polarity == PWM_POLARITY_ACTIVE_LOW) {
            // Select falling-edge sequence polarity for an active-low output.
            m_channel_polarity_mask[i] = PWM_VALUE_POLARITY_MASK;

            // Keep the GPIO idle level high while the PWM peripheral is inactive.
            nrfx_config->output_pins[i] |= NRFX_PWM_PIN_INVERTED;
        }
    }
}

static bool pwm_driver_start(nrfx_pwm_config_t const *config) {
    // Initialize the nrfx driver before configuring and starting playback.
    if (nrfx_pwm_init(&m_pwm, config, pwm_event_handler) != NRFX_SUCCESS)
        return false;

    m_pwm_sequence.values.p_individual = &m_sequence;

    // Individual load mode requires four 16-bit values, one for each channel.
    m_pwm_sequence.length = PWM_CHANNEL_COUNT;
    m_pwm_sequence.repeats = 0;
    m_pwm_sequence.end_delay = 0;

    // simple_playback assigns the same sequence to SEQ0 and SEQ1.
    // Both end events must be enabled and handled for looping playback.
    if (nrfx_pwm_simple_playback(
            &m_pwm,
            &m_pwm_sequence,
            1,
            NRFX_PWM_FLAG_LOOP |
            NRFX_PWM_FLAG_SIGNAL_END_SEQ0 |
            NRFX_PWM_FLAG_SIGNAL_END_SEQ1 |
            NRFX_PWM_FLAG_NO_EVT_FINISHED) != NRFX_SUCCESS) {
        // Release the driver if playback could not be started.
        nrfx_pwm_uninit(&m_pwm);
        return false;
    }

    return true;
}

static void pwm_event_handler(nrfx_pwm_evt_type_t event) {
    // Ignore events unrelated to the end of a sequence.
    if (event != NRFX_PWM_EVT_END_SEQ0 && event != NRFX_PWM_EVT_END_SEQ1)
        return;

    // Wait until the configured number of PWM periods has elapsed.
    if (++m_update_counter < m_update_interval)
        return;

    // Reset the interval counter and update the sequence buffer.
    m_update_counter = 0;
    pwm_sequence_update(&m_sequence, m_duty, m_top_value, m_channel_polarity_mask);
}
