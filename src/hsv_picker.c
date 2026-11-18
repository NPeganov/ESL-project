#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"

#include "btn/btn.h"
#include "leds/leds_control.h"
#include "log/log.h"
#include "timer_service/timer_service.h"

#include "hsv_picker.h"

// Device ID used to derive the initial Hue value.
#define DEVICE_ID "6609"

typedef enum {
    MODE_NO_INPUT,
    MODE_HUE,
    MODE_SATURATION,
    MODE_BRIGHTNESS
} hsv_picker_mode_t;

static hsv_picker_mode_t m_mode;
static bool m_parameter_changing;

static void button_event_handler(btn_event_t event);
static void mode_advance(void);
static void mode_set(hsv_picker_mode_t mode);
static leds_control_hsv_parameter_t mode_get_parameter(hsv_picker_mode_t mode);
static leds_control_yellow_pattern_t mode_get_yellow_pattern(hsv_picker_mode_t mode);
static char const *mode_get_name(hsv_picker_mode_t mode);
static uint16_t initial_hue_get(void);

bool hsv_picker_init(void) {
    if (!timer_service_init())
        return false;

    if (!leds_control_init())
        return false;

    m_mode = MODE_NO_INPUT;
    m_parameter_changing = false;

    leds_control_set_hsv(initial_hue_get(), 100, 100);
    btn_init(button_event_handler);

    return true;
}

void hsv_picker_start(void) {
    if (!leds_control_set_yellow_pattern(
            LEDS_CONTROL_YELLOW_OFF))
        APP_ERROR_HANDLER(NRF_ERROR_INTERNAL);

    btn_start();
}

static void button_event_handler(btn_event_t event) {
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;

    // This callback runs in interrupt context; logging is deferred by the logger.
    switch (event) {
        case BTN_EVENT_PRESS:
            if (m_mode == MODE_NO_INPUT || m_parameter_changing)
                break;

            if (!leds_control_start_hsv_change(
                    mode_get_parameter(m_mode))) {
                LOG_ERROR("Failed to start HSV parameter change");
                break;
            }

            m_parameter_changing = true;
            leds_control_get_hsv(&hue, &saturation, &value);

            LOG_INFO(
                "HSV start: H=%u S=%u V=%u",
                (unsigned int)hue,
                (unsigned int)saturation,
                (unsigned int)value);
            break;

        case BTN_EVENT_RELEASE:
            if (!m_parameter_changing)
                break;

            leds_control_stop_hsv_change();
            m_parameter_changing = false;
            leds_control_get_hsv(&hue, &saturation, &value);

            LOG_INFO(
                "HSV final: H=%u S=%u V=%u",
                (unsigned int)hue,
                (unsigned int)saturation,
                (unsigned int)value);
            break;

        case BTN_EVENT_DOUBLE_PRESS:
            mode_advance();
            break;

        case BTN_EVENT_SINGLE_PRESS:
        default:
            break;
    }
}

static void mode_advance(void) {
    switch (m_mode) {
        case MODE_NO_INPUT:
            mode_set(MODE_HUE);
            break;

        case MODE_HUE:
            mode_set(MODE_SATURATION);
            break;

        case MODE_SATURATION:
            mode_set(MODE_BRIGHTNESS);
            break;

        case MODE_BRIGHTNESS:
        default:
            mode_set(MODE_NO_INPUT);
            break;
    }
}

static void mode_set(hsv_picker_mode_t mode) {
    if (!leds_control_set_yellow_pattern(mode_get_yellow_pattern(mode)))
        APP_ERROR_HANDLER(NRF_ERROR_INTERNAL);

    m_mode = mode;

    LOG_INFO("HSV mode: %s", mode_get_name(m_mode));
}

static leds_control_hsv_parameter_t mode_get_parameter(hsv_picker_mode_t mode) {
    switch (mode) {
        case MODE_HUE:
            return LEDS_CONTROL_HUE;

        case MODE_SATURATION:
            return LEDS_CONTROL_SATURATION;

        case MODE_BRIGHTNESS:
        case MODE_NO_INPUT:
        default:
            return LEDS_CONTROL_VALUE;
    }
}

static leds_control_yellow_pattern_t mode_get_yellow_pattern(
    hsv_picker_mode_t mode) {
    switch (mode) {
        case MODE_HUE:
            return LEDS_CONTROL_YELLOW_SLOW_BLINK;

        case MODE_SATURATION:
            return LEDS_CONTROL_YELLOW_FAST_BLINK;

        case MODE_BRIGHTNESS:
            return LEDS_CONTROL_YELLOW_ON;

        case MODE_NO_INPUT:
        default:
            return LEDS_CONTROL_YELLOW_OFF;
    }
}

static char const *mode_get_name(hsv_picker_mode_t mode) {
    switch (mode) {
        case MODE_HUE:
            return "HUE";

        case MODE_SATURATION:
            return "SATURATION";

        case MODE_BRIGHTNESS:
            return "BRIGHTNESS";

        case MODE_NO_INPUT:
        default:
            return "NO_INPUT";
    }
}

static uint16_t initial_hue_get(void) {
    uint8_t length = sizeof(DEVICE_ID) - 1;
    uint8_t percent =
        (DEVICE_ID[length - 2] - '0') * 10 +
        (DEVICE_ID[length - 1] - '0');

    return (uint16_t)(percent * 360 / 100);
}
