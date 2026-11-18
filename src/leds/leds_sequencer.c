#include <stddef.h>

#include "app_error.h"
#include "app_timer.h"
#include "app_util_platform.h"
#include "nrf_assert.h"

#include "internal/sequencer_executor.h"
#include "leds_sequencer.h"

#define SEQUENCER_TIMER_INTERVAL_MS 10

static executor_t m_yellow_executor;
static executor_t m_rgb_executor;

static bool m_initialized;

APP_TIMER_DEF(m_timer);

static executor_t *executor_get(sequencer_led_id_t id);
static void timer_callback(void *p_context);

bool sequencer_init(void) {
    ASSERT(!m_initialized);

    if (m_initialized) return false;

    if (app_timer_create(
            &m_timer,
            APP_TIMER_MODE_REPEATED,
            timer_callback) != NRF_SUCCESS)
        return false;

    if (app_timer_start(
            m_timer,
            APP_TIMER_TICKS(SEQUENCER_TIMER_INTERVAL_MS),
            NULL) != NRF_SUCCESS)
        return false;

    m_initialized = true;

    return true;
}

bool sequencer_start(
    sequencer_led_id_t id,
    sequencer_config_t const *config,
    sequencer_start_mode_t start_mode,
    uint16_t current_value,
    sequencer_callback_t callback,
    void *context) {
    executor_t *executor = executor_get(id);

    if (!m_initialized || executor == NULL)
        return false;

    CRITICAL_REGION_ENTER();

    bool result = executor_start(
        executor,
        config,
        start_mode,
        current_value,
        callback,
        context);

    CRITICAL_REGION_EXIT();

    return result;
}

void sequencer_pause(sequencer_led_id_t id) {
    executor_t *executor = executor_get(id);

    if (executor == NULL) return;

    CRITICAL_REGION_ENTER();
    executor_pause(executor);
    CRITICAL_REGION_EXIT();
}

void sequencer_resume(sequencer_led_id_t id) {
    executor_t *executor = executor_get(id);

    if (executor == NULL) return;

    CRITICAL_REGION_ENTER();
    executor_resume(executor);
    CRITICAL_REGION_EXIT();
}

sequencer_state_t sequencer_get_state(sequencer_led_id_t id) {
    executor_t *executor = executor_get(id);
    sequencer_state_t state;

    if (executor == NULL) return SEQUENCER_STATE_IDLE;

    CRITICAL_REGION_ENTER();
    state = executor_get_state(executor);
    CRITICAL_REGION_EXIT();

    return state;
}

static executor_t *executor_get(sequencer_led_id_t id) {
    switch (id) {
        case SEQUENCER_YELLOW_LED:
            return &m_yellow_executor;
        case SEQUENCER_RGB_LED:
            return &m_rgb_executor;
        default:
            return NULL;
    }
}

static void timer_callback(void *p_context) {
    (void)p_context;

    executor_process(&m_yellow_executor);
    executor_process(&m_rgb_executor);
}
