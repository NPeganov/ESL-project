#ifndef _FLASH_STORAGE_LOG_H_
#define _FLASH_STORAGE_LOG_H_

#include <stdbool.h>
#include <stdint.h>

// Initialize the single-page log and scan its existing entries.
// free_value_mask: identifies an unset cell; valid data must not match this pattern.
// The log can be initialized only once.
bool flash_storage_log_init(uint32_t free_value_mask);

// Append one 32-bit value to the log.
// When the page is full, it is erased before the new value is written.
// Return false if the log is not initialized, the value is reserved, or the write fails.
bool flash_storage_log_save(uint32_t data);

// Load the most recently saved value.
// Return false if data is NULL, the log is not initialized, or no value is available.
bool flash_storage_log_load(uint32_t *data);

#endif
