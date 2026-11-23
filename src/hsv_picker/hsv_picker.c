#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"

#include "btn/btn.h"
#include "leds_control/leds_control.h"
#include "log/log.h"
#include "timer_service/timer_service.h"

// Double press selects an HSV component; holding the button changes it.
// The yellow LED indicates the selected mode.


// Last two device-ID digits determine the initial Hue percentage.
#define DEVICE_ID "6609"

// MODE_NO_INPUT disables HSV changes until a mode is selected.
typedef enum {
    MODE_NO_INPUT,
    MODE_HUE,
    MODE_SATURATION,
    MODE_BRIGHTNESS
} hsv_picker_mode_t;

// Selected input mode and active-change state.
static hsv_picker_mode_t m_mode;
static bool m_changing;

// Button event handling.
static void button_event_handler(btn_event_t event);
static void button_press(void);
static void button_release(void);

// Input mode and logging helpers.
static void mode_advance(void);
static void set_yellow_pattern(lc_yellow_pattern_t pattern);
static bool get_mode_parameter(lc_hsv_parameter_t *parameter);
static void log_parameter(char const *event, lc_hsv_parameter_t parameter);

// Initial device-specific HSV configuration.
static uint16_t get_initial_hue(void);


// Initialize dependencies and set the device-specific initial color.
// Button processing is enabled separately by hsv_picker_start().
bool hsv_picker_init(void) {
    if (!timer_service_init()) return false;

    if (!lc_init()) return false;

    // Button holds do not change HSV until an input mode is selected.
    m_mode = MODE_NO_INPUT;
    m_changing = false;

    // Start with the device-specific Hue and full saturation/value.
    lc_set_hsv(get_initial_hue(), 100, 100);

    btn_init(button_event_handler);

    return true;
}

// Turn off the mode indicator and enable button events.
void hsv_picker_start(void) {
    set_yellow_pattern(LC_YELLOW_OFF);

    btn_start();
}

// Route button events. Single-press is ignored; press/release control HSV,
// and double-press selects the next input mode. Logging is deferred.
static void button_event_handler(btn_event_t event) {
    switch (event) {
        case BTN_EVENT_PRESS: button_press(); break;
        case BTN_EVENT_RELEASE: button_release(); break;
        case BTN_EVENT_DOUBLE_PRESS: mode_advance(); break;
        case BTN_EVENT_SINGLE_PRESS:
        default: break;
    }
}

// Start changing the selected component unless a change is active or no
// input mode is selected. The mode cannot advance before this press releases.
static void button_press(void) {
    lc_hsv_parameter_t parameter;

    if (m_changing || !get_mode_parameter(&parameter)) return;

    if (!lc_start_hsv_change(parameter)) {
        LOG_ERROR("Failed to start HSV parameter change");
        return;
    }

    m_changing = true;
    log_parameter("started", parameter);
}

// Pause the active change. Ignore releases that did not start one.
static void button_release(void) {
    lc_hsv_parameter_t parameter;

    if (!m_changing || !get_mode_parameter(&parameter)) return;

    lc_stop_hsv_change();
    m_changing = false;
    log_parameter("finished", parameter);
}

// Apply a valid pattern after initialization; failure means an internal
// leds_control contract has been violated.
static void set_yellow_pattern(lc_yellow_pattern_t pattern) {
    if (!lc_set_yellow_pattern(pattern))
        APP_ERROR_HANDLER(NRF_ERROR_INTERNAL);
}

// Cycle HUE -> SATURATION -> BRIGHTNESS -> NO_INPUT. The yellow LED uses
// slow blink, fast blink, on, and off to indicate these modes.
static void mode_advance(void) {
    switch (m_mode) {
        case MODE_NO_INPUT:
            m_mode = MODE_HUE;
            set_yellow_pattern(LC_YELLOW_SLOW_BLINK);
            LOG_INFO("HSV-picker mode changed to HUE");
            break;

        case MODE_HUE:
            m_mode = MODE_SATURATION;
            set_yellow_pattern(LC_YELLOW_FAST_BLINK);
            LOG_INFO("HSV-picker mode changed to SATURATION");
            break;

        case MODE_SATURATION:
            m_mode = MODE_BRIGHTNESS;
            set_yellow_pattern(LC_YELLOW_ON);
            LOG_INFO("HSV-picker mode changed to BRIGHTNESS");
            break;

        case MODE_BRIGHTNESS:
        default:
            m_mode = MODE_NO_INPUT;
            set_yellow_pattern(LC_YELLOW_OFF);
            LOG_INFO("HSV-picker mode changed to NO_INPUT");
            break;
    }
}

// Return the HSV parameter for the current mode, or false for NO_INPUT.
static bool get_mode_parameter(lc_hsv_parameter_t *parameter) {
    switch (m_mode) {
        case MODE_HUE: *parameter = LC_HUE; return true;
        case MODE_SATURATION: *parameter = LC_SATURATION; return true;
        case MODE_BRIGHTNESS: *parameter = LC_VALUE; return true;
        case MODE_NO_INPUT:
        default: return false;
    }
}

// Log the selected component's value when its change starts or finishes.
// BRIGHTNESS is the user-facing name for HSV Value.
static void log_parameter(char const *event, lc_hsv_parameter_t parameter) {
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;

    lc_get_hsv(&hue, &saturation, &value);

    switch (parameter) {
        case LC_HUE:
            LOG_INFO("HUE change %s at %u degrees", event, (unsigned int)hue);
            break;

        case LC_SATURATION:
            LOG_INFO("SATURATION change %s at %u%%", event, (unsigned int)saturation);
            break;

        case LC_VALUE:
            LOG_INFO("BRIGHTNESS change %s at %u%%", event, (unsigned int)value);
            break;
    }
}

// Convert the last two device-ID digits from a percentage to Hue degrees.
// Integer division truncates; DEVICE_ID "6609" produces 32 degrees.
static uint16_t get_initial_hue(void) {
    uint8_t length = sizeof(DEVICE_ID) - 1;
    uint8_t percent =
        (DEVICE_ID[length - 2] - '0') * 10 +
        (DEVICE_ID[length - 1] - '0');

    return (uint16_t)(percent * 360 / 100);
}
