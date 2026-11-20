#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"

#include "btn/btn.h"
#include "leds_control/leds_control.h"
#include "log/log.h"
#include "timer_service/timer_service.h"

// HSV picker application logic.
//
// A double press selects which HSV component is controlled. While the button
// is held, the selected component changes continuously; releasing the button
// pauses the change. The yellow LED indicates the selected input mode.


// Device ID used to derive the initial Hue value. Its last two digits are
// interpreted as a percentage of the full Hue range (00..99 -> 0..356).
#define DEVICE_ID "6609"

// Input mode selected by double pressing the button. MODE_NO_INPUT disables
// HSV changes until the next mode is selected.
typedef enum {
    MODE_NO_INPUT,
    MODE_HUE,
    MODE_SATURATION,
    MODE_BRIGHTNESS
} hsv_picker_mode_t;

// Current input mode and whether an HSV change is currently running.
static hsv_picker_mode_t m_mode;
static bool m_changing;

// Button event handling.
static void button_event_handler(btn_event_t event);
static void button_press(void);
static void button_release(void);

// Input mode and logging helpers.
static void mode_advance(void);
static bool get_mode_parameter(lc_hsv_parameter_t *parameter);
static void log_parameter(char const *event, lc_hsv_parameter_t parameter);

// Initial device-specific HSV configuration.
static uint16_t get_initial_hue(void);


// Initialize the services and modules used by the HSV picker.
//
// The timer service must be initialized before leds_control and the button
// module because both depend on it. The initial HSV values are applied here,
// but button processing is enabled later by hsv_picker_start().
bool hsv_picker_init(void) {
    if (!timer_service_init())
        return false;

    if (!lc_init())
        return false;

    // Start in a mode where button holds do not change the HSV values.
    m_mode = MODE_NO_INPUT;
    m_changing = false;

    // Use the device ID to give each device a reproducible initial Hue.
    // Full saturation and value make the initial color vivid.
    lc_set_hsv(get_initial_hue(), 100, 100);

    btn_init(button_event_handler);

    return true;
}

// Start user interaction after successful initialization.
//
// The yellow LED is turned off to indicate that no HSV input mode is
// selected. Button events are enabled only after the initial LED state is set.
void hsv_picker_start(void) {
    if (!lc_set_yellow_pattern(LC_YELLOW_OFF))
        APP_ERROR_HANDLER(NRF_ERROR_INTERNAL);

    btn_start();
}

// Dispatch button events to the corresponding picker actions.
//
// This callback runs in interrupt context. The logger defers log processing,
// so logging from this callback does not perform the output operation here.
// A single-press event is intentionally ignored: press and release events
// control the HSV change, while a double-press event selects the next mode.
static void button_event_handler(btn_event_t event) {
    switch (event) {
        case BTN_EVENT_PRESS: button_press(); break;
        case BTN_EVENT_RELEASE: button_release(); break;
        case BTN_EVENT_DOUBLE_PRESS: mode_advance(); break;
        case BTN_EVENT_SINGLE_PRESS:
        default: break;
    }
}

// Start changing the HSV component selected by the current input mode.
//
// Ignore a press if a change is already active or if no input mode is
// selected. The selected component is obtained at press time; the button
// module reports the matching release before it reports a completed
// double-press event, so the mode remains consistent for this press cycle.
static void button_press(void) {
    lc_hsv_parameter_t parameter;

    if (m_changing || !get_mode_parameter(&parameter))
        return;

    if (!lc_start_hsv_change(parameter)) {
        LOG_ERROR("Failed to start HSV parameter change");
        return;
    }

    m_changing = true;
    log_parameter("started", parameter);
}

// Pause the active HSV change when the button is released.
//
// Do nothing if no change was started. The component is resolved from the
// current mode, which is still the same mode that was active on press.
static void button_release(void) {
    lc_hsv_parameter_t parameter;

    if (!m_changing || !get_mode_parameter(&parameter))
        return;

    lc_stop_hsv_change();
    m_changing = false;
    log_parameter("finished", parameter);
}

// Advance to the next input mode and update the yellow LED indication.
//
// The mode order is HUE -> SATURATION -> BRIGHTNESS -> NO_INPUT. Each mode
// has a distinct yellow LED pattern: slow blink, fast blink, steady on, and
// off respectively. A failure to apply the indicator pattern is treated as
// an internal application error.
static void mode_advance(void) {
    switch (m_mode) {
        case MODE_NO_INPUT:
            m_mode = MODE_HUE;
            if (!lc_set_yellow_pattern(LC_YELLOW_SLOW_BLINK))
                APP_ERROR_HANDLER(NRF_ERROR_INTERNAL);
            LOG_INFO("HSV-picker mode changed to HUE");
            break;

        case MODE_HUE:
            m_mode = MODE_SATURATION;
            if (!lc_set_yellow_pattern(LC_YELLOW_FAST_BLINK))
                APP_ERROR_HANDLER(NRF_ERROR_INTERNAL);
            LOG_INFO("HSV-picker mode changed to SATURATION");
            break;

        case MODE_SATURATION:
            m_mode = MODE_BRIGHTNESS;
            if (!lc_set_yellow_pattern(LC_YELLOW_ON))
                APP_ERROR_HANDLER(NRF_ERROR_INTERNAL);
            LOG_INFO("HSV-picker mode changed to BRIGHTNESS");
            break;

        case MODE_BRIGHTNESS:
        default:
            m_mode = MODE_NO_INPUT;
            if (!lc_set_yellow_pattern(LC_YELLOW_OFF))
                APP_ERROR_HANDLER(NRF_ERROR_INTERNAL);
            LOG_INFO("HSV-picker mode changed to NO_INPUT");
            break;
    }
}

// Translate the current picker mode to the leds_control HSV parameter.
//
// Return false when no input mode is selected. The output parameter is
// written only for modes that support HSV adjustment.
static bool get_mode_parameter(lc_hsv_parameter_t *parameter) {
    switch (m_mode) {
        case MODE_HUE: *parameter = LC_HUE; return true;
        case MODE_SATURATION: *parameter = LC_SATURATION; return true;
        case MODE_BRIGHTNESS: *parameter = LC_VALUE; return true;
        case MODE_NO_INPUT:
        default: return false;
    }
}

// Log the selected component's current value when a change starts or ends.
//
// The event argument is expected to be "started" or "finished". Read the
// complete HSV state and log only the component associated with the active
// input mode. BRIGHTNESS is the user-facing name for the HSV Value component.
static void log_parameter(char const *event, lc_hsv_parameter_t parameter) {
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;

    lc_get_hsv(&hue, &saturation, &value);

    switch (parameter) {
        case LC_HUE:
            LOG_INFO("HUE change %s at value %u", event, (unsigned int)hue);
            break;

        case LC_SATURATION:
            LOG_INFO("SATURATION change %s at value %u", event, (unsigned int)saturation);
            break;

        case LC_VALUE:
            LOG_INFO("BRIGHTNESS change %s at value %u", event, (unsigned int)value);
            break;
    }
}

// Calculate the initial Hue from the last two decimal digits of DEVICE_ID.
//
// Those digits form a percentage of the 360-degree Hue range. Integer
// arithmetic intentionally truncates the result to the nearest lower
// integer degree; for DEVICE_ID "6609", the initial Hue is 32 degrees.
static uint16_t get_initial_hue(void) {
    uint8_t length = sizeof(DEVICE_ID) - 1;
    uint8_t percent =
        (DEVICE_ID[length - 2] - '0') * 10 +
        (DEVICE_ID[length - 1] - '0');

    return (uint16_t)(percent * 360 / 100);
}
