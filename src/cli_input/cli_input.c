#include <stdbool.h>
#include <stdint.h>

#include "usb_port/usb_port.h"

#include "cli_input.h"

#define CLI_INPUT_BUFFER_SIZE 64

static char m_line[CLI_INPUT_BUFFER_SIZE];
static uint32_t m_line_size;
static bool m_ignore_lf;
static cli_input_line_handler_t m_line_handler;

void cli_input_init(cli_input_line_handler_t line_handler) {
    m_line_handler = line_handler;
    m_line_size = 0;
    m_ignore_lf = false;
}

void cli_input_receive(uint8_t byte) {
    if (byte == '\r' || byte == '\n') {
        if (byte == '\n' && m_ignore_lf)
            return;

        cli_input_write((uint8_t const *)"\r\n", 2);

        m_line[m_line_size] = '\0';
        m_line_handler(m_line);
        m_line_size = 0;
        m_ignore_lf = byte == '\r';
        return;
    }

    m_ignore_lf = false;

    if (m_line_size < CLI_INPUT_BUFFER_SIZE - 1) {
        m_line[m_line_size++] = (char)byte;
        cli_input_write(&byte, 1);
    }
}

void cli_input_write(uint8_t const * data, uint32_t size) {
    usb_port_write(data, size);
}
