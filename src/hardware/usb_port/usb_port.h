#ifndef _USB_PORT_H_
#define _USB_PORT_H_

#include <stdbool.h>
#include <stdint.h>

// Called when a byte is received from the USB port.
typedef void (*usb_port_rx_handler_t)(uint8_t byte);

// Initializes the USB CDC ACM port.
// The USB logger must be initialized first.
bool usb_port_init(usb_port_rx_handler_t rx_handler);

// Queues data for transmission through the USB port.
void usb_port_write(uint8_t const * data, uint32_t size);

#endif
