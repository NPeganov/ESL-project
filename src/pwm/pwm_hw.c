#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nrf_assert.h"
#include "nrf_gpio.h"
#include "nrfx_pwm.h"

#include "pwm_hw.h"

// The PWM top_value range is 3 to 32767.
// The maximum value is limited by the 15-bit compare field.
#define PWM_TOP_VALUE_MIN 3
#define PWM_TOP_VALUE_MAX 32767

// The PWM peripheral derives all base clocks from a 16 MHz source.
#define PWM_BASE_CLOCK_HZ 16000000

// The slowest supported base clock is 16 MHz divided by 128 (125 kHz).
#define PWM_CLOCK_DIVIDER_MAX 128

// Minimum frequency uses the slowest clock and largest top_value.
// Round up because a lower frequency would require an unsupported top_value.
#define PWM_FREQUENCY_MIN_HZ \
    ((PWM_BASE_CLOCK_HZ / PWM_CLOCK_DIVIDER_MAX + PWM_TOP_VALUE_MAX - 1) / PWM_TOP_VALUE_MAX)

// Maximum frequency uses the fastest clock and smallest supported top_value.
#define PWM_FREQUENCY_MAX_HZ (PWM_BASE_CLOCK_HZ / PWM_TOP_VALUE_MIN)

// Keep the PWM interrupt at the same priority as other application peripherals.
#define PWM_IRQ_PRIORITY 6

// Recalculate the output sequence every 10 ms.
#define PWM_UPDATE_INTERVAL_MS 10
#define PWM_MILLISECONDS_PER_SECOND 1000

// Bit 15 of a PWM sequence value selects the output polarity.
#define PWM_VALUE_POLARITY_MASK 0x8000

// Timing values selected for the requested PWM frequency.
typedef struct {
    nrf_pwm_clk_t base_clock;
    uint16_t top_value;
    uint32_t actual_frequency_hz;
} pwm_timing_t;

// Hardware PWM instance used by this module.
static nrfx_pwm_t m_pwm = NRFX_PWM_INSTANCE(0);

// Per-channel polarity bits added to the PWM sequence values.
static uint16_t m_channel_polarity_mask[PWM_CHANNEL_COUNT];

// Buffer containing the compare values for all four PWM channels.
static nrf_pwm_values_individual_t m_sequence;

// Sequence descriptor passed to the nrfx PWM driver.
static nrf_pwm_sequence_t m_pwm_sequence;

// Duty values owned by the public PWM module and read by the event handler.
static volatile uint8_t const *m_duty;

// Selected PWM period count, shared by all channels.
static uint16_t m_top_value;

// Number of PWM periods between sequence updates.
static uint32_t m_update_interval;

// PWM periods elapsed since the last sequence update.
static uint32_t m_update_counter;

// Select clock and top_value settings that best represent the requested frequency.
static bool pwm_timing_select(uint32_t frequency_hz, pwm_timing_t *timing);

// Validate channel settings and prepare the nrfx output pin configuration.
static void pwm_channels_configure(
    pwm_config_t const *config,
    nrfx_pwm_config_t *nrfx_config);

// Convert one channel's duty value to the corresponding PWM sequence value.
static uint16_t pwm_channel_value(pwm_channel_t channel);

// Refresh the compare values for all PWM channels from the current duty values.
static void pwm_sequence_update(void);

// Initialize the nrfx driver and start continuous playback of the PWM sequence.
static bool pwm_driver_start(nrfx_pwm_config_t const *config);

// Handle sequence-end events and update the PWM buffer at the configured interval.
static void pwm_event_handler(nrfx_pwm_evt_type_t event);

bool pwm_hw_init(pwm_config_t const *config, volatile uint8_t const *duty) {
    pwm_timing_t timing;
    nrfx_pwm_config_t nrfx_config;

    ASSERT(config != NULL);
    ASSERT(duty != NULL);

    m_duty = duty;

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
    pwm_sequence_update();

    return pwm_driver_start(&nrfx_config);
}

// Select the fastest PWM clock that can represent the requested frequency.
// A faster clock allows a larger top_value and therefore finer duty-cycle steps.
// Clock settings are ordered from fastest to slowest.
static bool pwm_timing_select(uint32_t frequency_hz, pwm_timing_t *timing) {
    // Reject frequencies that cannot be represented by any supported clock and top_value.
    if (frequency_hz < PWM_FREQUENCY_MIN_HZ ||
        frequency_hz > PWM_FREQUENCY_MAX_HZ)
        return false;

    uint32_t clock_frequency_hz = PWM_BASE_CLOCK_HZ;
    nrf_pwm_clk_t base_clock = NRF_PWM_CLK_16MHz;

    while (base_clock <= NRF_PWM_CLK_125kHz) {
        // Round the required period count to the nearest integer.
        uint32_t top_value =
            (clock_frequency_hz + frequency_hz / 2) / frequency_hz;

        // Use the first clock whose period count fits the hardware limits.
        if (top_value >= PWM_TOP_VALUE_MIN && top_value <= PWM_TOP_VALUE_MAX) {
            timing->base_clock = base_clock;
            timing->top_value = (uint16_t)top_value;

            // Store the frequency that these integer timing settings actually produce.
            timing->actual_frequency_hz =
                (clock_frequency_hz + top_value / 2) / top_value;

            return true;
        }

        // Try the next slower clock.
        base_clock = (nrf_pwm_clk_t)(base_clock + 1);
        clock_frequency_hz /= 2;
    }

    // No supported clock produced a valid period count.
    return false;
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

static uint16_t pwm_channel_value(pwm_channel_t channel) {
    // Convert the 0..255 duty value to active PWM ticks, rounded to nearest.
    uint32_t active_ticks =
        ((uint32_t)m_duty[channel] * m_top_value + UINT8_MAX / 2) / UINT8_MAX;

    // The compare value is the inactive part of the period; bit 15 selects polarity.
    uint16_t value = m_top_value - active_ticks;
    return value | m_channel_polarity_mask[channel];
}

static void pwm_sequence_update(void) {
    // NRF_PWM_LOAD_INDIVIDUAL stores one value for each of the four channels.
    m_sequence.channel_0 = pwm_channel_value(PWM_CHANNEL_0);
    m_sequence.channel_1 = pwm_channel_value(PWM_CHANNEL_1);
    m_sequence.channel_2 = pwm_channel_value(PWM_CHANNEL_2);
    m_sequence.channel_3 = pwm_channel_value(PWM_CHANNEL_3);
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
    pwm_sequence_update();
}
