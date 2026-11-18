#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app_error.h"
#include "app_timer.h"
#include "app_util_platform.h"
#include "nrf_assert.h"

#include "internal/interpol.h"
#include "leds_sequencer.h"

#define SEQUENCER_TIMER_INTERVAL_MS 10

typedef struct {
    sequencer_state_t state;

    sequencer_config_t config;
    sequencer_callback_t callback;
    void *context;

    uint16_t step_index;

    // Start value is used only while the first step is active.
    uint16_t start_value;
    bool start_value_pending;

    // Start time of the current step in app_timer ticks.
    uint32_t start_time;

    // Elapsed time saved when the executor is paused.
    uint32_t elapsed_time;

    uint16_t last_value;
    bool last_value_valid;
} executor_t;

// Each LED system has its own executor and can run independently.
static executor_t m_yellow_executor;
static executor_t m_rgb_executor;

static bool m_initialized;

APP_TIMER_DEF(m_timer);

static executor_t *executor_get(sequencer_led_id_t id);
static bool config_is_valid(sequencer_config_t const *config);
static bool config_has_timed_step(sequencer_config_t const *config);
static void timer_callback(void *p_context);
static void executor_process(executor_t *executor);
static bool executor_advance(executor_t *executor);
static void executor_set_value(executor_t *executor, uint16_t value);
static void executor_clear(executor_t *executor);

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

    if (!m_initialized || executor == NULL ||
        !config_is_valid(config) || callback == NULL)
        return false;

    if (start_mode != SEQUENCER_START_FROM_STEP &&
        start_mode != SEQUENCER_START_CURRENT)
        return false;

    if (config->repeat && !config_has_timed_step(config))
        return false;

    if ((config->scale == SEQUENCER_SCALE_CYCLIC ||
         config->scale == SEQUENCER_SCALE_BOUNCE) &&
        (!config->repeat || config->step_count != 1 ||
         config->steps[0].duration_ms == 0))
        return false;

    if (config->scale == SEQUENCER_SCALE_CYCLIC &&
        start_mode == SEQUENCER_START_CURRENT && current_value >= 360)
        return false;

    if (config->scale == SEQUENCER_SCALE_BOUNCE &&
        start_mode == SEQUENCER_START_CURRENT &&
        (current_value < config->steps[0].from ||
         current_value > config->steps[0].to))
        return false;

    // Keep the timer handler from observing a partially replaced executor.
    CRITICAL_REGION_ENTER();

    executor->config = *config;
    executor->callback = callback;
    executor->context = context;
    executor->step_index = 0;
    executor->start_value = current_value;
    executor->start_value_pending = (start_mode == SEQUENCER_START_CURRENT);
    executor->start_time = app_timer_cnt_get();
    executor->elapsed_time = 0;
    executor->last_value_valid = false;
    executor->state = SEQUENCER_STATE_RUNNING;

    executor_process(executor);

    CRITICAL_REGION_EXIT();

    return true;
}

void sequencer_pause(sequencer_led_id_t id) {
    executor_t *executor = executor_get(id);

    if (executor == NULL) return;

    CRITICAL_REGION_ENTER();

    if (executor->state == SEQUENCER_STATE_RUNNING) {
        executor->elapsed_time = app_timer_cnt_diff_compute(
            app_timer_cnt_get(),
            executor->start_time);

        executor->state = SEQUENCER_STATE_PAUSED;
    }

    CRITICAL_REGION_EXIT();
}

void sequencer_resume(sequencer_led_id_t id) {
    executor_t *executor = executor_get(id);

    if (executor == NULL) return;

    CRITICAL_REGION_ENTER();

    if (executor->state == SEQUENCER_STATE_PAUSED) {
        executor->start_time = app_timer_cnt_get() - executor->elapsed_time;
        executor->state = SEQUENCER_STATE_RUNNING;
    }

    CRITICAL_REGION_EXIT();
}

sequencer_state_t sequencer_get_state(sequencer_led_id_t id) {
    executor_t *executor = executor_get(id);

    sequencer_state_t state;

    if (executor == NULL) return SEQUENCER_STATE_IDLE;

    CRITICAL_REGION_ENTER();
    state = executor->state;
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

static bool config_is_valid(sequencer_config_t const *config) {
    if (config == NULL) return false;
    if (config->steps == NULL) return false;
    if (config->step_count == 0) return false;

    if (config->scale != SEQUENCER_SCALE_LINEAR &&
        config->scale != SEQUENCER_SCALE_CYCLIC &&
        config->scale != SEQUENCER_SCALE_BOUNCE)
        return false;

    if (config->scale == SEQUENCER_SCALE_CYCLIC &&
        (config->steps[0].from != 0 || config->steps[0].to != 360))
        return false;

    if (config->scale == SEQUENCER_SCALE_BOUNCE &&
        config->steps[0].from >= config->steps[0].to)
        return false;

    return true;
}

static bool config_has_timed_step(sequencer_config_t const *config) {
    for (uint16_t i = 0; i < config->step_count; i++)
        if (config->steps[i].duration_ms > 0) return true;

    return false;
}

static void timer_callback(void *p_context) {
    (void)p_context;

    executor_process(&m_yellow_executor);
    executor_process(&m_rgb_executor);
}

static void executor_process(executor_t *executor) {
    uint32_t now;

    if (executor->state != SEQUENCER_STATE_RUNNING) return;

    now = app_timer_cnt_get();

    while (executor->state == SEQUENCER_STATE_RUNNING) {
        sequencer_step_t const *step = &executor->config.steps[executor->step_index];
        uint32_t duration = APP_TIMER_TICKS(step->duration_ms);
        uint32_t elapsed = app_timer_cnt_diff_compute(now, executor->start_time);
        uint16_t start_value = executor->start_value_pending ?
            executor->start_value : step->from;

        if (executor->config.scale == SEQUENCER_SCALE_BOUNCE) {
            executor_set_value(
                executor,
                interpol_value(
                    executor->config.scale,
                    step->from,
                    step->to,
                    start_value,
                    elapsed,
                    duration));

            return;
        }

        if (duration == 0) {
            executor_set_value(executor, step->to);

            if (!executor_advance(executor))
                return;

            continue;
        }

        if (elapsed >= duration) {
            if (executor->config.scale == SEQUENCER_SCALE_CYCLIC)
                executor_set_value(
                    executor,
                    interpol_value(
                        executor->config.scale,
                        step->from,
                        step->to,
                        start_value,
                        duration,
                        duration));
            else
                executor_set_value(executor, step->to);

            executor->start_time += duration;

            if (!executor_advance(executor))
                return;

            continue;
        }

        executor_set_value(
            executor,
            interpol_value(
                executor->config.scale,
                step->from,
                step->to,
                start_value,
                elapsed,
                duration));

        return;
    }
}

static bool executor_advance(executor_t *executor) {
    if (executor->step_index == 0)
        executor->start_value_pending = false;

    executor->step_index++;

    if (executor->step_index < executor->config.step_count)
        return true;

    if (!executor->config.repeat) {
        executor_clear(executor);
        return false;
    }

    executor->step_index = 0;

    return true;
}

static void executor_set_value(executor_t *executor, uint16_t value) {
    if (executor->last_value_valid && executor->last_value == value) return;

    executor->last_value = value;
    executor->last_value_valid = true;

    executor->callback(value, executor->context);
}

static void executor_clear(executor_t *executor) {
    executor->state = SEQUENCER_STATE_IDLE;
    executor->config = (sequencer_config_t){ 0 };
    executor->callback = NULL;
    executor->context = NULL;
    executor->step_index = 0;
    executor->start_value = 0;
    executor->start_value_pending = false;
    executor->start_time = 0;
    executor->elapsed_time = 0;
    executor->last_value_valid = false;
}
