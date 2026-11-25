#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"
#include "app_util_platform.h"

#include "btn/btn.h"
#include "leds_control/leds_control.h"
#include "log/log.h"
#include "nvm_log/nvm_log.h"
#include "timer_service/timer_service.h"

// DOUBLE_PRESS selects an HSV component; HOLD starts changing it until RELEASE.
// The yellow LED indicates the selected mode.

// Last two device-ID digits determine the initial Hue percentage.
#define DEVICE_ID "6609"
#define HSV_MAX_HUE 359
#define HSV_MAX_SATURATION 100
#define HSV_MAX_VALUE 100
// Identify erased flash cells without matching any valid HSV value.
#define HSV_FREE_VALUE_MASK (~((uint32_t)HSV_MAX_HUE | ((uint32_t)HSV_MAX_SATURATION << 16) | ((uint32_t)HSV_MAX_VALUE << 24)))

typedef enum {
    MODE_NO_INPUT,
    MODE_HUE,
    MODE_SATURATION,
    MODE_BRIGHTNESS
} hsv_picker_mode_t;

// Selected input mode and active-change state.
static hsv_picker_mode_t m_mode;
static bool m_hsv_change_active;
static lc_change_parameter_t m_active_parameter;
static volatile bool m_save_pending;

// Button event handling.
static void button_event_handler(btn_event_t event);
static void button_hold(void);
static void button_release(void);

// Input mode and logging helpers.
static void mode_advance(void);
static void set_yellow_pattern(lc_yellow_pattern_t pattern);
static void log_parameter(char const *event, lc_change_parameter_t parameter);

// Initial device-specific HSV configuration.
static uint16_t get_initial_hue(void);
static bool load_saved_hsv(void);
static void save_current_hsv(void);


// Initialize dependencies and set the device-specific initial color.
// Button processing starts later, after the initial indication is prepared.
bool hsv_picker_init(void) {
    if (!nvm_log_init(HSV_FREE_VALUE_MASK)) return false;
    if (!timer_service_init()) return false;
    if (!lc_init()) return false;

    // Button holds do not change HSV until an input mode is selected.
    m_mode = MODE_NO_INPUT;
    m_hsv_change_active = false;
    m_save_pending = false;

    // Use the saved color when available; otherwise derive the default from DEVICE_ID.
    if (!load_saved_hsv()) {
        uint16_t hue = get_initial_hue();

        lc_set_hsv(hue, HSV_MAX_SATURATION, HSV_MAX_VALUE);
        LOG_INFO("No valid saved HSV color; using DEVICE_ID %s: H=%u S=%u V=%u",
                 DEVICE_ID, (unsigned int)hue,
                 (unsigned int)HSV_MAX_SATURATION, (unsigned int)HSV_MAX_VALUE);
    }

    return true;
}

// Prepare the initial indication and then start button processing.
void hsv_picker_start(void) {
    set_yellow_pattern(LC_YELLOW_OFF);

    // Start the button only after the picker state and indication are ready.
    btn_init(button_event_handler);
}


// Process flash writes outside interrupt context.
void hsv_picker_process(void) {
    bool save_pending;

    CRITICAL_REGION_ENTER();
    save_pending = m_save_pending;
    m_save_pending = false;
    CRITICAL_REGION_EXIT();

    if (save_pending) save_current_hsv();
}

// Route button events. Hold starts HSV changes, release finishes them,
// and double-press selects the next input mode. Single-press is ignored.
// Logging is deferred.
static void button_event_handler(btn_event_t event) {
    switch (event) {
        case BTN_EVENT_HOLD: button_hold(); break;
        case BTN_EVENT_RELEASE: button_release(); break;
        case BTN_EVENT_DOUBLE_PRESS: mode_advance(); break;
        case BTN_EVENT_PRESS:
        case BTN_EVENT_SINGLE_PRESS:
        default: break;
    }
}

// Start changing the selected component unless a change is already active
// or no input mode is selected.
static void button_hold(void) {
    // Ignore repeated holds and holds in NO_INPUT mode.
    if (m_hsv_change_active || m_mode == MODE_NO_INPUT) return;

    lc_change_parameter_t parameter = LC_HUE;

    switch (m_mode) {
        case MODE_NO_INPUT: return;
        case MODE_HUE: parameter = LC_HUE; break;
        case MODE_SATURATION: parameter = LC_SATURATION; break;
        case MODE_BRIGHTNESS: parameter = LC_VALUE; break;
    }

    lc_start_hsv_change(parameter);

    m_active_parameter = parameter;
    m_hsv_change_active = true;
    log_parameter("started", m_active_parameter);
}

// Handle button release and stop any active HSV change.
static void button_release(void) {
    // Nothing to do if no HSV change is active.
    if (!m_hsv_change_active) return;

    lc_stop_hsv_change();
    m_hsv_change_active = false;
    log_parameter("finished", m_active_parameter);
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
            m_save_pending = true;
            LOG_INFO("HSV-picker mode changed to NO_INPUT");
            break;
    }
}

// Log the selected component's value when its change starts or finishes.
// BRIGHTNESS is the user-facing name for HSV Value.
static void log_parameter(char const *event, lc_change_parameter_t parameter) {
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

// Restore the last saved HSV color when the record contains valid values.
static bool load_saved_hsv(void) {
    uint32_t data;
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;

    if (!nvm_log_load(&data)) return false;

    // Unpack the stored HSV components.
    hue = (uint16_t)data;
    saturation = (uint8_t)(data >> 16);
    value = (uint8_t)(data >> 24);

    if (hue >= 360 || saturation > HSV_MAX_SATURATION || value > HSV_MAX_VALUE)
        return false;

    lc_set_hsv(hue, saturation, value);
    LOG_INFO("Restored HSV color: H=%u S=%u V=%u",
             (unsigned int)hue, (unsigned int)saturation, (unsigned int)value);

    return true;
}

// Append the current HSV color to the flash log.
static void save_current_hsv(void) {
    uint32_t data;
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;

    lc_get_hsv(&hue, &saturation, &value);

    // Pack HSV components into one word: H[15:0], S[23:16], V[31:24].
    data = (uint32_t)hue |
           ((uint32_t)saturation << 16) |
           ((uint32_t)value << 24);

    if (!nvm_log_save(data)) {
        LOG_ERROR("Failed to save HSV color");
        return;
    }

    LOG_INFO("Saved HSV color: H=%u S=%u V=%u",
             (unsigned int)hue, (unsigned int)saturation, (unsigned int)value);
}
