#include <stdbool.h>

#include "app_error.h"
#include "nrf.h"

#include "hsv_picker.h"
#include "log/log.h"

int main(void) {
    log_init();

    if (!hsv_picker_init())
        APP_ERROR_HANDLER(NRF_ERROR_INTERNAL);

    hsv_picker_start();

    while (true) {
        log_process();
        __WFE();
    }
}
