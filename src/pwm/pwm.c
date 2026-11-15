#include <stddef.h>
#include <stdint.h>

#include "nrf_assert.h"
#include "nrf_gpio.h"
#include "nrfx_pwm.h"

#include "pwm.h"

// The nRF52840 PWM COUNTERTOP register accepts values from 3 to 32767.
// The upper limit is 2^15 - 1 because COUNTERTOP is a 15-bit field.
#define PWM_COUNTERTOP_MIN 3
#define PWM_COUNTERTOP_MAX 32767
#define PWM_IRQ_PRIORITY 6
#define PWM_UPDATE_INTERVAL_MS 10
#define PWM_MILLISECONDS_PER_SECOND 1000

typedef struct {
    uint32_t frequency_hz;
    nrf_pwm_clk_t clock;
} pwm_clock_t;

typedef struct {
    nrf_pwm_clk_t base_clock;
    uint16_t top_value;
    uint32_t actual_frequency_hz;
} pwm_timing_t;

static nrfx_pwm_t m_pwm = NRFX_PWM_INSTANCE(0);

static pwm_polarity_t m_channel_polarity[PWM_CHANNEL_COUNT];
static nrf_pwm_values_individual_t m_sequence;
static nrf_pwm_sequence_t m_pwm_sequence;

static volatile uint8_t m_duty[PWM_CHANNEL_COUNT];
static uint16_t m_top_value;
static uint32_t m_update_interval;
static uint32_t m_update_counter;

static void pwm_timing_calculate(uint32_t frequency_hz, pwm_timing_t *timing);
static uint16_t pwm_channel_value(pwm_channel_t channel);
static void pwm_sequence_update(void);
static void pwm_event_handler(nrfx_pwm_evt_type_t event);

void pwm_init(pwm_config_t const *config) {
    pwm_timing_t timing;
    nrfx_pwm_config_t nrfx_config;

    ASSERT(config != NULL);

    nrfx_config.irq_priority = PWM_IRQ_PRIORITY;
    nrfx_config.count_mode = NRF_PWM_MODE_UP;
    nrfx_config.load_mode = NRF_PWM_LOAD_INDIVIDUAL;
    nrfx_config.step_mode = NRF_PWM_STEP_AUTO;

    pwm_timing_calculate(config->frequency_hz, &timing);

    m_top_value = timing.top_value;
    nrfx_config.base_clock = timing.base_clock;
    nrfx_config.top_value = timing.top_value;

    for (uint8_t i = 0; i < PWM_CHANNEL_COUNT; i++) {
        ASSERT(nrf_gpio_pin_present_check(config->channels[i].pin));
        ASSERT(config->channels[i].polarity == PWM_POLARITY_ACTIVE_HIGH ||
               config->channels[i].polarity == PWM_POLARITY_ACTIVE_LOW);

        m_channel_polarity[i] = config->channels[i].polarity;
        m_duty[i] = 0;
        nrfx_config.output_pins[i] = config->channels[i].pin;

        if (m_channel_polarity[i] == PWM_POLARITY_ACTIVE_LOW)
            nrfx_config.output_pins[i] |= NRFX_PWM_PIN_INVERTED;
    }

    // Convert the update interval from milliseconds to PWM periods.
    m_update_interval = (
        timing.actual_frequency_hz * PWM_UPDATE_INTERVAL_MS +
        PWM_MILLISECONDS_PER_SECOND / 2) / PWM_MILLISECONDS_PER_SECOND;

    // At least one PWM period must pass before the sequence can be updated.
    if (m_update_interval == 0)
        m_update_interval = 1;

    pwm_sequence_update();

    // Keep the function call outside ASSERT because ASSERT may omit its expression.
    nrfx_err_t err_code = nrfx_pwm_init(&m_pwm, &nrfx_config, pwm_event_handler);
    ASSERT(err_code == NRFX_SUCCESS);

    m_pwm_sequence.values.p_individual = &m_sequence;
    m_pwm_sequence.length = PWM_CHANNEL_COUNT;
    m_pwm_sequence.repeats = 0;
    m_pwm_sequence.end_delay = 0;

    // simple_playback uses the same sequence for SEQ0 and SEQ1.
    // Handle both end events as required by the driver API.
    // Keep the function call outside ASSERT because ASSERT may omit its expression.
    err_code = nrfx_pwm_simple_playback(
        &m_pwm,
        &m_pwm_sequence,
        1,
        NRFX_PWM_FLAG_LOOP |
        NRFX_PWM_FLAG_SIGNAL_END_SEQ0 |
        NRFX_PWM_FLAG_SIGNAL_END_SEQ1 |
        NRFX_PWM_FLAG_NO_EVT_FINISHED);
    ASSERT(err_code == NRFX_SUCCESS);
}

void pwm_set(pwm_channel_t channel, uint8_t duty) {
    ASSERT((uint32_t)channel < PWM_CHANNEL_COUNT);

    m_duty[channel] = duty;
}

static void pwm_timing_calculate(uint32_t frequency_hz, pwm_timing_t *timing) {
    static const pwm_clock_t clocks[] = {
        { 16000000, NRF_PWM_CLK_16MHz },
        { 8000000, NRF_PWM_CLK_8MHz },
        { 4000000, NRF_PWM_CLK_4MHz },
        { 2000000, NRF_PWM_CLK_2MHz },
        { 1000000, NRF_PWM_CLK_1MHz },
        { 500000, NRF_PWM_CLK_500kHz },
        { 250000, NRF_PWM_CLK_250kHz },
        { 125000, NRF_PWM_CLK_125kHz }
    };

    size_t const clock_count = sizeof(clocks) / sizeof(clocks[0]);

    ASSERT(timing != NULL);
    ASSERT(frequency_hz != 0);

    // Try clocks from fastest to slowest to get the highest available resolution.
    for (size_t i = 0; i < clock_count; i++) {
        // COUNTERTOP is the number of PWM clock ticks in one PWM period.
        // Round to the nearest integer because COUNTERTOP must be a whole number.
        uint32_t top_value =
            (clocks[i].frequency_hz + frequency_hz / 2) / frequency_hz;

        if (top_value < PWM_COUNTERTOP_MIN || top_value > PWM_COUNTERTOP_MAX)
            continue;

        timing->base_clock = clocks[i].clock;
        timing->top_value = (uint16_t)top_value;

        // Calculate the frequency produced by the selected clock and COUNTERTOP.
        timing->actual_frequency_hz =
            (clocks[i].frequency_hz + top_value / 2) / top_value;

        return;
    }

    ASSERT(0);

    // Use the slowest clock and minimum COUNTERTOP as a safe fallback.
    timing->base_clock = clocks[clock_count - 1].clock;
    timing->top_value = PWM_COUNTERTOP_MIN;
    timing->actual_frequency_hz =
        (clocks[clock_count - 1].frequency_hz + PWM_COUNTERTOP_MIN / 2) /
        PWM_COUNTERTOP_MIN;
}

static uint16_t pwm_channel_value(pwm_channel_t channel) {
    uint32_t active_ticks = ((uint32_t)m_duty[channel] * m_top_value + 127) / 255;
    uint16_t value = m_top_value - active_ticks;

    if (m_channel_polarity[channel] == PWM_POLARITY_ACTIVE_LOW)
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
    if (event != NRFX_PWM_EVT_END_SEQ0 && event != NRFX_PWM_EVT_END_SEQ1)
        return;

    if (++m_update_counter < m_update_interval)
        return;

    m_update_counter = 0;
    pwm_sequence_update();
}
