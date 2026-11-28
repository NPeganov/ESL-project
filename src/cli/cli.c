#include <stdbool.h>

#include "cli_input/cli_input.h"
#include "usb_port/usb_port.h"

#include "cli.h"

static void cli_receive(char const * line) {
    (void)line;
}

bool cli_init(void) {
    cli_input_init(cli_receive);
    return usb_port_init(cli_input_receive);
}
