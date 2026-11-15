#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nrf_assert.h"
#include "nrf_gpio.h"
#include "nrfx_pwm.h"

#include "pwm.h"

// The nRF52840 PWM period register accepts values from 3 to 32767.
// The maximum value is 2^15 - 1 because the register field is 15 bits.
#define PWM_COUNTERTOP_MIN 3
#define PWM_COUNTERTOP_MAX 32767
#define PWM_BASE_CLOCK_HZ 16000000
#define PWM_CLOCK_DIVIDER_MAX 128
#define PWM_FREQUENCY_MIN_HZ \
    ((PWM_BASE_CLOCK_HZ / PWM_CLOCK_DIVIDER_MAX + PWM_COUNTERTOP_MAX - 1) / PWM_COUNTERTOP_MAX)
#define PWM_FREQUENCY_MAX_HZ (PWM_BASE_CLOCK_HZ / PWM_COUNTERTOP_MIN)
#define PWM_IRQ_PRIORITY 6
#define PWM_UPDATE_INTERVAL_MS 10
#define PWM_MILLISECONDS_PER_SECOND 1000
#define PWM_VALUE_POLARITY_MASK 0x8000

typedef struct {
    nrf_pwm_clk_t base_clock;
    uint16_t top_value;
    uint32_t actual_frequency_hz;
} pwm_timing_t;

static nrfx_pwm_t m_pwm = NRFX_PWM_INSTANCE(0);

static uint16_t m_channel_polarity_mask[PWM_CHANNEL_COUNT];
static nrf_pwm_values_individual_t m_sequence;
static nrf_pwm_sequence_t m_pwm_sequence;

static volatile uint8_t m_duty[PWM_CHANNEL_COUNT];
static uint16_t m_top_value;
static uint32_t m_update_interval;
static uint32_t m_update_counter;

static bool pwm_timing_select(uint32_t frequency_hz, pwm_timing_t *timing);
static void pwm_channels_configure(
    pwm_config_t const *config,
    nrfx_pwm_config_t *nrfx_config);
static bool pwm_driver_start(nrfx_pwm_config_t const *config);
static uint16_t pwm_channel_value(pwm_channel_t channel);
static void pwm_sequence_update(void);
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

    // Convert the update interval from milliseconds to PWM periods.
    m_update_interval = (
        timing.actual_frequency_hz * PWM_UPDATE_INTERVAL_MS +
        PWM_MILLISECONDS_PER_SECOND / 2) / PWM_MILLISECONDS_PER_SECOND;

    // At least one PWM period must pass before the sequence can be updated.
    if (m_update_interval == 0)
        m_update_interval = 1;

    pwm_sequence_update();

    return pwm_driver_start(&nrfx_config);
}

void pwm_set(pwm_channel_t channel, uint8_t duty) {
    ASSERT((uint32_t)channel < PWM_CHANNEL_COUNT);

    m_duty[channel] = duty;
}

// Select the fastest PWM clock that can represent the requested frequency.
// A faster clock allows a larger top_value and therefore finer duty-cycle steps.
// Clock settings are ordered from fastest to slowest, with each step halving the clock.
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
        if (top_value >= PWM_COUNTERTOP_MIN && top_value <= PWM_COUNTERTOP_MAX) {
            timing->base_clock = base_clock;
            timing->top_value = (uint16_t)top_value;

            // Store the frequency that these integer timing settings actually produce.
            timing->actual_frequency_hz =
                (clock_frequency_hz + top_value / 2) / top_value;

            return true;
        }

        // Try the next, half-speed clock to bring top_value into the supported range.
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
        ASSERT(nrf_gpio_pin_present_check(config->channels[i].pin));
        ASSERT(config->channels[i].polarity == PWM_POLARITY_ACTIVE_HIGH ||
               config->channels[i].polarity == PWM_POLARITY_ACTIVE_LOW);

        m_channel_polarity_mask[i] = 0;
        m_duty[i] = 0;
        nrfx_config->output_pins[i] = config->channels[i].pin;

        if (config->channels[i].polarity == PWM_POLARITY_ACTIVE_LOW) {
            m_channel_polarity_mask[i] = PWM_VALUE_POLARITY_MASK;
            nrfx_config->output_pins[i] |= NRFX_PWM_PIN_INVERTED;
        }
    }
}

static bool pwm_driver_start(nrfx_pwm_config_t const *config) {
    if (nrfx_pwm_init(&m_pwm, config, pwm_event_handler) != NRFX_SUCCESS)
        return false;

    m_pwm_sequence.values.p_individual = &m_sequence;
    m_pwm_sequence.length = PWM_CHANNEL_COUNT;
    m_pwm_sequence.repeats = 0;
    m_pwm_sequence.end_delay = 0;

    // simple_playback uses the same sequence for SEQ0 and SEQ1.
    // Handle both end events as required by the driver API.
    if (nrfx_pwm_simple_playback(
            &m_pwm,
            &m_pwm_sequence,
            1,
            NRFX_PWM_FLAG_LOOP |
            NRFX_PWM_FLAG_SIGNAL_END_SEQ0 |
            NRFX_PWM_FLAG_SIGNAL_END_SEQ1 |
            NRFX_PWM_FLAG_NO_EVT_FINISHED) != NRFX_SUCCESS) {
        nrfx_pwm_uninit(&m_pwm);
        return false;
    }

    return true;
}

static uint16_t pwm_channel_value(pwm_channel_t channel) {
    uint32_t active_ticks =
        ((uint32_t)m_duty[channel] * m_top_value + UINT8_MAX / 2) / UINT8_MAX;

    uint16_t value = m_top_value - active_ticks;
    return value | m_channel_polarity_mask[channel];
}

static void pwm_sequence_update(void) {
    m_sequence.channel_0 = pwm_channel_value(PWM_CHANNEL_0);
    m_sequence.channel_1 = pwm_channel_value(PWM_CHANNEL_1);
    m_sequence.channel_2 = pwm_channel_value(PWM_CHANNEL_2);
    m_sequence.channel_3 = pwm_channel_value(PWM_CHANNEL_3);
}

static void pwm_event_handler(nrfx_pwm_evt_type_t event) {
    if (event != NRFX_PWM_EVT_END_SEQ0 && event != NRFX_PWM_EVT_END_SEQ1)
        return;

    if (++m_update_counter < m_update_interval)
        return;

    m_update_counter = 0;
    pwm_sequence_update();
}
