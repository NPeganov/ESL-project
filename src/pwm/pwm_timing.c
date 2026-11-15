#include <stdbool.h>
#include <stdint.h>

#include "nrfx_pwm.h"

#include "pwm_internal.h"

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

// Select the fastest PWM clock that can represent the requested frequency.
// A faster clock allows a larger top_value and therefore finer duty-cycle steps.
// Clock settings are ordered from fastest to slowest.
bool pwm_timing_select(uint32_t frequency_hz, pwm_timing_t *timing) {
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
