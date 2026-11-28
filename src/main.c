#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"

#include "hsv_picker/hsv_picker.h"
#include "hsv_picker_cli/hsv_picker_cli.h"
#include "tools/usb_logger/usb_logger.h"

int main(void) {
    usb_logger_init();

    // Initialize the HSV Picker command-line interface.
    if (!hsv_picker_cli_init())
        APP_ERROR_HANDLER(NRF_ERROR_INTERNAL);

    // Initialize the HSV Picker and prepare its initial indication.
    if (!hsv_picker_init())
        APP_ERROR_HANDLER(NRF_ERROR_INTERNAL);

    // Enable user interaction.
    hsv_picker_start();

    // Process deferred flash writes and log messages, then sleep until an event.
    while (true) {
        hsv_picker_process();
        usb_logger_process();
        __WFE();
    }
}
