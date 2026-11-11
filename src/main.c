#include <stdbool.h>

#include "nrfx_systick.h"

#include "btn/btn.h"
#include "leds/leds.h"
#include "leds/led_sequence.h"
#include "log/log.h"


#define DEVICE_ID               "6609"

#define PWM_PERIOD_US           1000
#define FADE_DURATION_MS        600
#define CYCLE_PAUSE_MS          500
#define INTER_LED_PAUSE_MS      500


static void system_init(led_sequence_t *sequence) {
    // Initialize LED GPIOs before any module that drives LED outputs.
    leds_init();

    // Initialize SysTick before any module that uses it as a time base.
    nrfx_systick_init();

    // Initialize shared LFCLK and app_timer required by the button debounce timer.
    // The clock driver is also reused by the USB logging backend.
    btn_init();

    // Initialize logging after btn_init() so the USB backend can reuse
    // the already initialized clock infrastructure.
    log_init();

    if (!led_sequence_init(
            sequence,
            DEVICE_ID,
            PWM_PERIOD_US,
            FADE_DURATION_MS,
            CYCLE_PAUSE_MS,
            INTER_LED_PAUSE_MS)) while (true);
}

int main(void) {
    led_sequence_t sequence;

    system_init(&sequence);

    while (true)
    {
        // Keep the LED sequence and software PWM serviced.
        led_sequence_process(&sequence);

        // Process USB events and pending log messages.
        log_process();

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
