#ifndef _USB_LOGGER_H_
#define _USB_LOGGER_H_

#include "nrf_log.h"

// Initializes the logger and all enabled backends.
void usb_logger_init(void);

// Processes USB events and pending deferred log messages.
// Must be called regularly from the main loop.
void usb_logger_process(void);

// Application logging interface.
#define LOG_INFO(...)    NRF_LOG_INFO(__VA_ARGS__)
#define LOG_WARNING(...) NRF_LOG_WARNING(__VA_ARGS__)
#define LOG_ERROR(...)   NRF_LOG_ERROR(__VA_ARGS__)

#endif
