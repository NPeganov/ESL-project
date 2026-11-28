#include <stdbool.h>

#include "tools/cli_input/cli_input.h"
#include "hardware/usb_port/usb_port.h"

#include "hsv_picker_cli.h"

static void cli_receive(char const * line) {
    (void)line;
}

bool hsv_picker_cli_init(void) {
    cli_input_init(cli_receive);
    return usb_port_init(cli_input_receive);
}
