#ifndef _CLI_INPUT_H_
#define _CLI_INPUT_H_

#include <stddef.h>
#include <stdint.h>

// Called when a complete input line is received.
typedef void (*cli_input_line_handler_t)(char const * line);

// Initializes the input line buffer.
void cli_input_init(cli_input_line_handler_t line_handler);

// Passes a block of received data to the input line buffer.
void cli_input_receive(uint8_t const * data, size_t size);

// Sends data through the underlying port.
void cli_input_write(uint8_t const * data, size_t size);

#endif
