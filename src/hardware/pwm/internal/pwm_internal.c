#include <stdbool.h>
#include <stdint.h>

#include "nrfx_pwm.h"

#include "pwm_internal.h"

// Hardware limits for the 15-bit compare value and prescaled PWM clocks.
#define PWM_TOP_VALUE_MIN 3
#define PWM_TOP_VALUE_MAX 32767
#define PWM_BASE_CLOCK_HZ 16000000
#define PWM_CLOCK_DIVIDER_MAX 128

#define PWM_FREQUENCY_MIN_HZ \
    ((PWM_BASE_CLOCK_HZ / PWM_CLOCK_DIVIDER_MAX + PWM_TOP_VALUE_MAX - 1) / PWM_TOP_VALUE_MAX)
#define PWM_FREQUENCY_MAX_HZ (PWM_BASE_CLOCK_HZ / PWM_TOP_VALUE_MIN)

// Bit 15 selects falling-edge polarity for an active-low output.
#define PWM_COMPARE_POLARITY_MASK 0x8000

// Choose a base clock and period count that represent the requested frequency.
bool pwm_timing_select(uint32_t frequency_hz, pwm_timing_t *timing) {
    // Check the overall hardware range before attempting clock selection.
    if (frequency_hz < PWM_FREQUENCY_MIN_HZ ||
        frequency_hz > PWM_FREQUENCY_MAX_HZ)
        return false;

    uint32_t clock_frequency_hz = PWM_BASE_CLOCK_HZ;
    nrf_pwm_clk_t base_clock = NRF_PWM_CLK_16MHz;

    // Try clocks from fastest to slowest, preferring the highest resolution.
    while (base_clock <= NRF_PWM_CLK_125kHz) {
        // Round to the nearest integer period count for the requested frequency.
        uint32_t top_value =
            (clock_frequency_hz + frequency_hz / 2) / frequency_hz;

        if (top_value >= PWM_TOP_VALUE_MIN && top_value <= PWM_TOP_VALUE_MAX) {
            timing->base_clock = base_clock;
            timing->top_value = (uint16_t)top_value;

            // Store the realizable frequency; it may differ slightly after rounding.
            timing->actual_frequency_hz =
                (clock_frequency_hz + top_value / 2) / top_value;
            return true;
        }

        // Advance to the next hardware clock and apply its divide-by-two rate.
        base_clock = (nrf_pwm_clk_t)(base_clock + 1);
        clock_frequency_hz /= 2;
    }

    // No supported clock produced a valid period count.
    return false;
}

// Convert 8-bit logical duty into the nRF PWM compare value for one channel.
static uint16_t pwm_channel_compare_value(
    uint8_t duty,
    uint16_t top_value,
    bool active_low) {

    // Scale 0..255 to 0..top_value, rounding to the nearest timer tick.
    uint32_t active_ticks =
        ((uint32_t)duty * top_value + UINT8_MAX / 2) / UINT8_MAX;

    // In up-counting mode, the compare value determines the inactive portion.
    uint16_t compare_value = top_value - active_ticks;

    // The high bit selects the edge polarity; the lower 15 bits hold compare.
    if (active_low)
        compare_value |= PWM_COMPARE_POLARITY_MASK;

    return compare_value;
}

// Rebuild all four values in the individual-load sequence from current duties.
void pwm_sequence_update(
    nrf_pwm_values_individual_t *sequence,
    volatile uint8_t const *duty,
    uint16_t top_value,
    bool const *active_low) {

    sequence->channel_0 = pwm_channel_compare_value(duty[0], top_value, active_low[0]);
    sequence->channel_1 = pwm_channel_compare_value(duty[1], top_value, active_low[1]);
    sequence->channel_2 = pwm_channel_compare_value(duty[2], top_value, active_low[2]);
    sequence->channel_3 = pwm_channel_compare_value(duty[3], top_value, active_low[3]);
}
