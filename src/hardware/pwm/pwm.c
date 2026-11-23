#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nrf_assert.h"
#include "nrf_gpio.h"
#include "nrfx_pwm.h"

#include "pwm.h"

#define PWM_IRQ_PRIORITY 6                         // Interrupt priority.
#define PWM_BASE_CLOCK NRF_PWM_CLK_1MHz            // Peripheral base clock.
#define PWM_BASE_CLOCK_HZ 1000000                  // Base clock frequency in Hz.
#define PWM_FREQUENCY_HZ 1000                      // PWM output frequency in Hz.
#define PWM_TOP_VALUE (PWM_BASE_CLOCK_HZ / PWM_FREQUENCY_HZ - 1) // Counter top value.
#define PWM_UPDATE_INTERVAL_MS 10                  // Duty update interval.
#define PWM_UPDATE_PERIODS (PWM_FREQUENCY_HZ * PWM_UPDATE_INTERVAL_MS / 1000) // PWM periods per update.
#define PWM_COMPARE_POLARITY_MASK 0x8000           // Compare value polarity bit.

static nrfx_pwm_t m_pwm = NRFX_PWM_INSTANCE(0);             // PWM peripheral instance.
static bool m_channel_active_low[PWM_CHANNEL_COUNT];       // Active-low channel flags.
static nrf_pwm_values_individual_t m_sequence_values;       // Values used by the playback sequence.
static volatile uint8_t m_duty[PWM_CHANNEL_COUNT];          // Latest requested channel duties.

// Configure channel pins, polarity, and initial duty values.
static void pwm_channels_configure(
    pwm_config_t const *config,
    nrfx_pwm_config_t *nrfx_config);

// Convert one logical duty value to the nRF PWM compare value.
static uint16_t pwm_channel_compare_value(
    uint8_t duty,
    bool active_low);

// Update all sequence compare values from the current duties.
static void pwm_sequence_update(void);

// Initialize the nrfx driver and start playback.
static bool pwm_driver_start(nrfx_pwm_config_t const *config);

// Refresh sequence values at the configured update interval.
static void pwm_playback_event_handler(nrfx_pwm_evt_type_t event);

// Initialize the fixed PWM timing and channels.
bool pwm_init(pwm_config_t const *config) {
    nrfx_pwm_config_t nrfx_config;

    ASSERT(config != NULL);

    // Fixed 1 MHz base clock and 1 kHz PWM frequency.
    nrfx_config.base_clock = PWM_BASE_CLOCK;
    nrfx_config.top_value = PWM_TOP_VALUE;
    nrfx_config.irq_priority = PWM_IRQ_PRIORITY;
    nrfx_config.count_mode = NRF_PWM_MODE_UP;
    nrfx_config.load_mode = NRF_PWM_LOAD_INDIVIDUAL;
    nrfx_config.step_mode = NRF_PWM_STEP_AUTO;

    pwm_channels_configure(config, &nrfx_config);
    pwm_sequence_update();

    return pwm_driver_start(&nrfx_config);
}

void pwm_set(pwm_channel_t channel, uint8_t duty) {
    ASSERT((uint32_t)channel < PWM_CHANNEL_COUNT);

    // The handler copies the latest duty into the PWM sequence.
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

        // Apply inversion for active-low physical outputs.
        if (m_channel_active_low[i])
            nrfx_config->output_pins[i] |= NRFX_PWM_PIN_INVERTED;
    }
}

static uint16_t pwm_channel_compare_value(
    uint8_t duty,
    bool active_low) {

    // Scale 0..255 to 0..top_value, rounding to the nearest tick.
    uint32_t active_ticks =
        ((uint32_t)duty * PWM_TOP_VALUE + UINT8_MAX / 2) / UINT8_MAX;

    // In up-counting mode, compare value defines the inactive time.
    uint16_t compare_value = PWM_TOP_VALUE - active_ticks;

    // The high bit selects the output polarity.
    if (active_low)
        compare_value |= PWM_COMPARE_POLARITY_MASK;

    return compare_value;
}

static void pwm_sequence_update(void) {
    m_sequence_values.channel_0 =
        pwm_channel_compare_value(m_duty[0], m_channel_active_low[0]);
    m_sequence_values.channel_1 =
        pwm_channel_compare_value(m_duty[1], m_channel_active_low[1]);
    m_sequence_values.channel_2 =
        pwm_channel_compare_value(m_duty[2], m_channel_active_low[2]);
    m_sequence_values.channel_3 =
        pwm_channel_compare_value(m_duty[3], m_channel_active_low[3]);
}

static bool pwm_driver_start(nrfx_pwm_config_t const *config) {
    nrf_pwm_sequence_t sequence;

    if (nrfx_pwm_init(&m_pwm, config, pwm_playback_event_handler) != NRFX_SUCCESS)
        return false;

    // One sequence is used for continuous playback.
    sequence.values.p_individual = &m_sequence_values;
    sequence.length = PWM_CHANNEL_COUNT;
    sequence.repeats = 0;
    sequence.end_delay = 0;

    // Signal both sequence boundaries used by simple playback.
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

    // Ignore events other than sequence-end events.
    if (event != NRFX_PWM_EVT_END_SEQ0 && event != NRFX_PWM_EVT_END_SEQ1)
        return;

    // Refresh the sequence every 10 ms.
    if (++periods_since_update < PWM_UPDATE_PERIODS)
        return;

    periods_since_update = 0;
    pwm_sequence_update();
}
