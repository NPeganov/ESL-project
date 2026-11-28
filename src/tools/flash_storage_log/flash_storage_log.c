#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nrfx_nvmc.h"

#include "flash_storage_log.h"

#define FLASH_STORAGE_LOG_PAGE_ADDRESS 0xDD000          // First Application Data Area page.
#define FLASH_STORAGE_LOG_PAGE_SIZE 4096                 // nRF52840 flash page size.
#define FLASH_STORAGE_LOG_WORD_SIZE sizeof(uint32_t)     // NVMC programming unit.
#define FLASH_STORAGE_LOG_WORD_COUNT (FLASH_STORAGE_LOG_PAGE_SIZE / FLASH_STORAGE_LOG_WORD_SIZE)

static uint32_t m_free_value_mask;             // Mask used to identify an unset cell.
static size_t m_used_cell_count;               // Number of occupied cells in the log page.
static size_t m_latest_index;                  // Index of the most recently saved cell.
static bool m_has_latest;                      // True when the log contains a saved value.
static bool m_initialized;                     // True after successful initialization.

static uint32_t get_cell_address_by_index(size_t index);
static uint32_t get_cell_value_by_index(size_t index);
static bool is_cell_value_unset(uint32_t value);
static bool write_cell(uint32_t address, uint32_t value);

// Initialize the log and count the occupied cells.
bool flash_storage_log_init(uint32_t free_value_mask) {
    if (m_initialized) return false;

    m_free_value_mask = free_value_mask;
    m_used_cell_count = 0;

    // Log entries are stored consecutively from the beginning of the page.
    // The first unset cell marks the end of the existing log.
    for (size_t i = 0; i < FLASH_STORAGE_LOG_WORD_COUNT; i++) {
        if (is_cell_value_unset(get_cell_value_by_index(i)))
            break;

        m_latest_index = i;
        m_has_latest = true;
        m_used_cell_count++;
    }

    m_initialized = true;

    return true;
}

// Append one value to the next free cell.
bool flash_storage_log_save(uint32_t data) {
    uint32_t address;

    // Values matching the unset-cell pattern are reserved and cannot be saved.
    if (!m_initialized ||
        is_cell_value_unset(data))
        return false;

    // Reuse the page from the beginning when every cell has been occupied.
    // Erasing the page also removes the previously saved entries.
    if (m_used_cell_count >= FLASH_STORAGE_LOG_WORD_COUNT) {
        if (nrfx_nvmc_page_erase(FLASH_STORAGE_LOG_PAGE_ADDRESS) != NRFX_SUCCESS)
            return false;

        m_used_cell_count = 0;
        m_has_latest = false;
    }

    address = get_cell_address_by_index(m_used_cell_count);

    if (!write_cell(address, data))
        return false;

    // Update the in-memory log state only after the flash write has completed.
    m_latest_index = m_used_cell_count;
    m_used_cell_count++;
    m_has_latest = true;

    return true;
}

// Load the most recently saved value.
bool flash_storage_log_load(uint32_t *data) {
    if (!m_initialized || data == NULL || !m_has_latest)
        return false;

    *data = get_cell_value_by_index(m_latest_index);

    return true;
}

// Calculate the flash address of a cell from its zero-based index.
static uint32_t get_cell_address_by_index(size_t index) {
    return FLASH_STORAGE_LOG_PAGE_ADDRESS + index * FLASH_STORAGE_LOG_WORD_SIZE;
}

// Read a cell directly from memory-mapped flash.
static uint32_t get_cell_value_by_index(size_t index) {
    return *(uint32_t const *)get_cell_address_by_index(index);
}

// A cell is unset when every bit selected by the caller-provided mask is set.
static bool is_cell_value_unset(uint32_t value) {
    return (value & m_free_value_mask) == m_free_value_mask;
}

// Program a cell only if its current flash contents allow the requested value.
static bool write_cell(uint32_t address, uint32_t value) {
    if (!nrfx_nvmc_word_writable_check(address, value))
        return false;

    nrfx_nvmc_word_write(address, value);

    // Wait until NVMC has finished programming the word.
    while (!nrfx_nvmc_write_done_check());

    return true;
}
