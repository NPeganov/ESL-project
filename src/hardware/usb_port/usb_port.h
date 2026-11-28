#ifndef _USB_PORT_H_
#define _USB_PORT_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Called with received data valid only for the duration of the callback.
typedef void (*usb_port_rx_handler_t)(uint8_t const * data, size_t size);

// Called when the host opens or closes the CDC port.
// A bus reset is reported as close/open when the host reopens the port.
typedef void (*usb_port_state_handler_t)(bool port_open);

// Initializes the USB CDC ACM port.
// The USB stack must be initialized, and event processing must not have started.
bool usb_port_init(usb_port_rx_handler_t rx_handler,
                   usb_port_state_handler_t state_handler);

// Queues the entire data block for transmission to the USB host.
// Returns false if the port is closed, arguments are invalid, or the queue is full.
// Returns true when accepted into internal buffers, not when transmission completes.
// Accepted data may be discarded when the host closes or resets the port.
// Must not be called from an interrupt handler.
bool usb_port_write(uint8_t const * data, size_t size);

#endif
