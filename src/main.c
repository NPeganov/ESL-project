#include <stdbool.h>

#include "app_error.h"
#include "nrf.h"

#include "hsv_picker/hsv_picker.h"
#include "log/log.h"

int main(void) {
    log_init();

    // Initialize the HSV Picker and prepare its initial indication.
    if (!hsv_picker_init())
        APP_ERROR_HANDLER(NRF_ERROR_INTERNAL);

    // Enable user interaction.
    hsv_picker_start();

    // Process deferred flash writes and log messages, then sleep until an event.
    while (true) {
        hsv_picker_process();
        log_process();
        __WFE();
    }
}
