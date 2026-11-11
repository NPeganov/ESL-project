#include <stdbool.h>

#include "nrfx_systick.h"

#include "btn/btn.h"
#include "leds/leds.h"
#include "leds/led_sequence.h"


#define DEVICE_ID               "6609"

#define PWM_PERIOD_US           1000
#define FADE_DURATION_MS        600
#define CYCLE_PAUSE_MS          500
#define INTER_LED_PAUSE_MS      500


int main(void) {
    led_sequence_t sequence;

    leds_init();

    nrfx_systick_init();

    btn_init();

    if (!led_sequence_init(
            &sequence,
            DEVICE_ID,
            PWM_PERIOD_US,
            FADE_DURATION_MS,
            CYCLE_PAUSE_MS,
            INTER_LED_PAUSE_MS)) while (true);

    while (true) {
        // Keep the LED sequence and software PWM serviced, including while paused.
        led_sequence_process(&sequence);

        // Process all pending button events.
        btn_event_t event;
        while ((event = btn_event_take()) != BTN_EVENT_NONE)
        {
            if (event != BTN_EVENT_DOUBLE_PRESS)
                continue;

            // Toggle the LED sequence on every double press.
            if (led_sequence_is_running(&sequence))
                led_sequence_pause(&sequence);
            else
                led_sequence_resume(&sequence);
        }
    }
}