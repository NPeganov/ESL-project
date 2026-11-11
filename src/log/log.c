#include "app_error.h"
#include "nrf_log.h"
#include "nrf_log_backend_usb.h"
#include "nrf_log_default_backends.h"

#include "log.h"

void log_init(void) {
    APP_ERROR_CHECK(NRF_LOG_INIT(NULL));
    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

void log_process(void) {
    LOG_BACKEND_USB_PROCESS();
    NRF_LOG_PROCESS();
}
