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
    MODE_BRIGHTNESS,
    MODE_COUNT
} hsv_picker_mode_t;

typedef struct {
    lc_hsv_parameter_t hsv_parameter;
    lc_yellow_pattern_t yellow_pattern;
    char const *mode_name;
} mode_config_t;

static mode_config_t const m_mode_config[] = {
    {LC_VALUE,      LC_YELLOW_OFF,        "NO_INPUT"},
    {LC_HUE,        LC_YELLOW_SLOW_BLINK, "HUE"},
    {LC_SATURATION, LC_YELLOW_FAST_BLINK, "SATURATION"},
    {LC_VALUE,      LC_YELLOW_ON,         "BRIGHTNESS"}
};

static hsv_picker_mode_t m_mode;
static bool m_changing;

static void button_event_handler(btn_event_t event);
static void button_press(void);
static void button_release(void);
static void mode_advance(void);
static void log_parameter(char const *event, lc_hsv_parameter_t parameter);
static uint16_t initial_hue_get(void);

bool hsv_picker_init(void) {
    if (!timer_service_init())
        return false;

    if (!lc_init())
        return false;

    m_mode = MODE_NO_INPUT;
    m_changing = false;

    lc_set_hsv(initial_hue_get(), 100, 100);
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
        case BTN_EVENT_PRESS:
            button_press();
            break;

        case BTN_EVENT_RELEASE:
            button_release();
            break;

        case BTN_EVENT_DOUBLE_PRESS:
            mode_advance();
            break;

        case BTN_EVENT_SINGLE_PRESS:
        default:
            break;
    }
}

static void button_press(void) {
    if (m_mode == MODE_NO_INPUT || m_changing)
        return;

    if (!lc_start_hsv_change(m_mode_config[m_mode].hsv_parameter)) {
        LOG_ERROR("Failed to start HSV parameter change");
        return;
    }

    m_changing = true;
    log_parameter("start", m_mode_config[m_mode].hsv_parameter);
}

static void button_release(void) {
    if (!m_changing)
        return;

    lc_stop_hsv_change();
    m_changing = false;
    log_parameter("final", m_mode_config[m_mode].hsv_parameter);
}

static void mode_advance(void) {
    m_mode = (m_mode + 1) % MODE_COUNT;

    if (!lc_set_yellow_pattern(m_mode_config[m_mode].yellow_pattern))
        APP_ERROR_HANDLER(NRF_ERROR_INTERNAL);

    LOG_INFO("HSV mode: %s", m_mode_config[m_mode].mode_name);
}

static void log_parameter(char const *event, lc_hsv_parameter_t parameter) {
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;

    lc_get_hsv(&hue, &saturation, &value);

    switch (parameter) {
        case LC_HUE:
            LOG_INFO("HSV %s: H=%u", event, (unsigned int)hue);
            break;

        case LC_SATURATION:
            LOG_INFO("HSV %s: S=%u", event, (unsigned int)saturation);
            break;

        case LC_VALUE:
            LOG_INFO("HSV %s: V=%u", event, (unsigned int)value);
            break;
    }
}

static uint16_t initial_hue_get(void) {
    uint8_t length = sizeof(DEVICE_ID) - 1;
    uint8_t percent =
        (DEVICE_ID[length - 2] - '0') * 10 +
        (DEVICE_ID[length - 1] - '0');

    return (uint16_t)(percent * 360 / 100);
}
