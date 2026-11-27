#include <stdbool.h>
#include <stdint.h>

#include "cli_input/cli_input.h"
#include "usb_port/usb_port.h"

#include "cli.h"

static void cli_receive(char const * line) {
    uint32_t size = 0;

    while (line[size] != '\0')
        size++;

    cli_input_write((uint8_t const *)line, size);
    cli_input_write((uint8_t const *)"\r\n", 2);
}

bool cli_init(void) {
    cli_input_init(cli_receive);
    return usb_port_init(cli_input_receive);
}
