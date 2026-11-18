#include <stdint.h>

#include "nrfx_systick.h"

#include "btn/btn.h"
#include "log/log.h"
#include "timer_service/timer_service.h"

static void button_event_handler(btn_event_t event);

int main(void) {
    log_init();
    nrfx_systick_init();

    if (!timer_service_init()) {
        LOG_ERROR("Button test: timer service initialization failed");
        while (1) {
            log_process();
            nrfx_systick_delay_ms(100);
        }
    }

    btn_init(button_event_handler);
    btn_start();

    LOG_INFO("Button test ready");

    while (1) {
        log_process();
        nrfx_systick_delay_ms(10);
    }
}

static void button_event_handler(btn_event_t event) {
    switch (event) {
        case BTN_EVENT_PRESS:
            LOG_INFO("Callback received: PRESS");
            break;

        case BTN_EVENT_RELEASE:
            LOG_INFO("Callback received: RELEASE");
            break;

        case BTN_EVENT_SINGLE_PRESS:
            LOG_INFO("Callback received: SINGLE_PRESS");
            break;

        case BTN_EVENT_DOUBLE_PRESS:
            LOG_INFO("Callback received: DOUBLE_PRESS");
            break;

        default:
            break;
    }
}
