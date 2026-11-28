#include <stdbool.h>

#include "app_timer.h"
#include "nrf_drv_clock.h"

#include "timer_service.h"

static bool m_initialized;

bool timer_service_init(void) {
    if (m_initialized) return true;

    // Initialize the shared legacy clock driver if needed.
    if (!nrf_drv_clock_init_check() &&
        nrf_drv_clock_init() != NRF_SUCCESS)
        return false;

    // Keep LFCLK running for app_timer.
    nrf_drv_clock_lfclk_request(NULL);

    while (!nrf_drv_clock_lfclk_is_running());

    if (app_timer_init() != NRF_SUCCESS)
        return false;

    m_initialized = true;

    return true;
}
