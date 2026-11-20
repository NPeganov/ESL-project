#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"

#include "btn/btn.h"
#include "leds_control/leds_control.h"
#include "log/log.h"
#include "timer_service/timer_service.h"

// Device ID used to derive the initial Hue value.
#define DEVICE_ID "6609"

typedef enum {
    MODE_NO_INPUT,
    MODE_HUE,
    MODE_SATURATION,
    MODE_BRIGHTNESS
} hsv_picker_mode_t;

static hsv_picker_mode_t m_mode;
static bool m_changing;

static void button_event_handler(btn_event_t event);
static void button_press(void);
static void button_release(void);
static void mode_advance(void);
static bool get_mode_parameter(lc_hsv_parameter_t *parameter);
static void log_parameter(char const *event, lc_hsv_parameter_t parameter);
static uint16_t get_initial_hue(void);

bool hsv_picker_init(void) {
    if (!timer_service_init())
        return false;

    if (!lc_init())
        return false;

    m_mode = MODE_NO_INPUT;
    m_changing = false;

    lc_set_hsv(get_initial_hue(), 100, 100);
    btn_init(button_event_handler);

    return true;
}

void hsv_picker_start(void) {
    if (!lc_set_yellow_pattern(LC_YELLOW_OFF))
        APP_ERROR_HANDLER(NRF_ERROR_INTERNAL);

    btn_start();
}

static void button_event_handler(btn_event_t event) {
    // This callback runs in interrupt context; logging is deferred by the logger.
    switch (event) {
        case BTN_EVENT_PRESS: button_press(); break;
        case BTN_EVENT_RELEASE: button_release(); break;
        case BTN_EVENT_DOUBLE_PRESS: mode_advance(); break;
        case BTN_EVENT_SINGLE_PRESS:
        default: break;
    }
}

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

static void button_release(void) {
    lc_hsv_parameter_t parameter;

    if (!m_changing || !get_mode_parameter(&parameter))
        return;

    lc_stop_hsv_change();
    m_changing = false;
    log_parameter("finished", parameter);
}

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

static bool get_mode_parameter(lc_hsv_parameter_t *parameter) {
    switch (m_mode) {
        case MODE_HUE: *parameter = LC_HUE; return true;
        case MODE_SATURATION: *parameter = LC_SATURATION; return true;
        case MODE_BRIGHTNESS: *parameter = LC_VALUE; return true;
        case MODE_NO_INPUT:
        default: return false;
    }
}

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

static uint16_t get_initial_hue(void) {
    uint8_t length = sizeof(DEVICE_ID) - 1;
    uint8_t percent =
        (DEVICE_ID[length - 2] - '0') * 10 +
        (DEVICE_ID[length - 1] - '0');

    return (uint16_t)(percent * 360 / 100);
}
