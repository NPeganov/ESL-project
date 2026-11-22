#include <stdbool.h>

#include "nrf_delay.h"

#include "hsv_picker/leds_control/leds_control.h"
#include "timer_service/timer_service.h"

#define HSV_CHANGE_DURATION_MS 5000
#define YELLOW_PATTERN_DURATION_MS 3000

// Demonstrate independent HSV changes and yellow LED indication patterns.
int main(void) {
    if (!timer_service_init())
        while (true);

    if (!lc_init())
        while (true);

    lc_set_hsv(0, 100, 100);
    lc_set_yellow_pattern(LC_YELLOW_SLOW_BLINK);

    while (true) {
        lc_start_hsv_change(LC_HUE);
        nrf_delay_ms(HSV_CHANGE_DURATION_MS);
        lc_stop_hsv_change();

        lc_start_hsv_change(LC_SATURATION);
        nrf_delay_ms(HSV_CHANGE_DURATION_MS);
        lc_stop_hsv_change();

        lc_start_hsv_change(LC_VALUE);
        nrf_delay_ms(HSV_CHANGE_DURATION_MS);
        lc_stop_hsv_change();

        // Restore a visible RGB color before testing the yellow LED patterns.
        lc_set_hsv(0, 100, 100);

        lc_set_yellow_pattern(LC_YELLOW_FAST_BLINK);
        nrf_delay_ms(YELLOW_PATTERN_DURATION_MS);

        lc_set_yellow_pattern(LC_YELLOW_ON);
        nrf_delay_ms(YELLOW_PATTERN_DURATION_MS);

        lc_set_yellow_pattern(LC_YELLOW_OFF);
        nrf_delay_ms(YELLOW_PATTERN_DURATION_MS);
    }
}
