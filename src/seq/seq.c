#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app_error.h"
#include "app_timer.h"

#include "interpol/interpol.h"
#include "seq.h"

#define SEQ_TIMER_INTERVAL_MS 10

typedef struct {
    seq_state_t state;

    seq_config_t config;
    seq_callback_t callback;
    void *context;

    uint16_t step_index;

    // Start time of the current step in app_timer ticks.
    uint32_t start_time;

    // Elapsed time saved when the executor is paused.
    uint32_t elapsed_time;

    uint16_t last_value;
    bool last_value_valid;
} seq_executor_t;

// Each LED system has its own executor and can run independently.
static seq_executor_t m_yellow_executor;
static seq_executor_t m_rgb_executor;

static bool m_initialized;

APP_TIMER_DEF(m_timer);

static seq_executor_t *executor_get(seq_led_id id);
static bool config_is_valid(seq_config_t const *config);
static bool config_has_timed_step(seq_config_t const *config);
static void timer_callback(void *p_context);
static void executor_process(seq_executor_t *executor);
static bool executor_advance(seq_executor_t *executor);
static void executor_set_value(seq_executor_t *executor, uint16_t value);
static void executor_clear(seq_executor_t *executor);

bool seq_init(void) {
    ASSERT(!m_initialized);

    if (m_initialized) return false;

    if (app_timer_create(
            &m_timer,
            APP_TIMER_MODE_REPEATED,
            timer_callback) != NRF_SUCCESS)
        return false;

    if (app_timer_start(
            m_timer,
            APP_TIMER_TICKS(SEQ_TIMER_INTERVAL_MS),
            NULL) != NRF_SUCCESS)
        return false;

    m_initialized = true;

    return true;
}

bool seq_start(
    seq_led_id id,
    seq_config_t const *config,
    seq_callback_t callback,
    void *context) {
    seq_executor_t *executor = executor_get(id);

    if (!m_initialized || executor == NULL || !config_is_valid(config))
        return false;

    if (config->repeat && !config_has_timed_step(config))
        return false;

    executor->config = *config;
    executor->callback = callback;
    executor->context = context;
    executor->step_index = 0;
    executor->start_time = app_timer_cnt_get();
    executor->elapsed_time = 0;
    executor->last_value_valid = false;
    executor->state = SEQ_STATE_RUNNING;

    executor_process(executor);

    return true;
}

void seq_pause(seq_led_id id) {
    seq_executor_t *executor = executor_get(id);

    if (executor == NULL || executor->state != SEQ_STATE_RUNNING) return;

    executor->elapsed_time = app_timer_cnt_diff_compute(
        app_timer_cnt_get(),
        executor->start_time);

    executor->state = SEQ_STATE_PAUSED;
}

void seq_resume(seq_led_id id) {
    seq_executor_t *executor = executor_get(id);

    if (executor == NULL || executor->state != SEQ_STATE_PAUSED) return;

    executor->start_time = app_timer_cnt_get() - executor->elapsed_time;
    executor->state = SEQ_STATE_RUNNING;
}

seq_state_t seq_get_state(seq_led_id id) {
    seq_executor_t *executor = executor_get(id);

    if (executor == NULL) return SEQ_STATE_IDLE;

    return executor->state;
}

static seq_executor_t *executor_get(seq_led_id id) {
    switch (id) {
        case YELLOW_LED:
            return &m_yellow_executor;
        case RGB_LED:
            return &m_rgb_executor;
        default:
            return NULL;
    }
}

static bool config_is_valid(seq_config_t const *config) {
    if (config == NULL) return false;
    if (config->steps == NULL) return false;
    if (config->step_count == 0) return false;

    return true;
}

static bool config_has_timed_step(seq_config_t const *config) {
    for (uint16_t i = 0; i < config->step_count; i++)
        if (config->steps[i].duration_ms > 0) return true;

    return false;
}

static void timer_callback(void *p_context) {
    (void)p_context;

    executor_process(&m_yellow_executor);
    executor_process(&m_rgb_executor);
}

static void executor_process(seq_executor_t *executor) {
    uint32_t now;

    if (executor->state != SEQ_STATE_RUNNING) return;

    now = app_timer_cnt_get();

    while (executor->state == SEQ_STATE_RUNNING) {
        seq_step_t const *step = &executor->config.steps[executor->step_index];
        uint32_t duration = APP_TIMER_TICKS(step->duration_ms);
        uint32_t elapsed = app_timer_cnt_diff_compute(now, executor->start_time);

        if (duration == 0) {
            executor_set_value(executor, step->to);

            if (!executor_advance(executor))
                return;

            continue;
        }

        if (elapsed >= duration) {
            executor_set_value(executor, step->to);
            executor->start_time += duration;

            if (!executor_advance(executor))
                return;

            continue;
        }

        executor_set_value(
            executor,
            interpol_linear(step->from, step->to, elapsed, duration));

        return;
    }
}

static bool executor_advance(seq_executor_t *executor) {
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

static void executor_set_value(seq_executor_t *executor, uint16_t value) {
    if (executor->last_value_valid && executor->last_value == value) return;

    executor->last_value = value;
    executor->last_value_valid = true;

    executor->callback(value, executor->context);
}

static void executor_clear(seq_executor_t *executor) {
    executor->state = SEQ_STATE_IDLE;
    executor->config = (seq_config_t){ 0 };
    executor->callback = NULL;
    executor->context = NULL;
    executor->step_index = 0;
    executor->start_time = 0;
    executor->elapsed_time = 0;
    executor->last_value_valid = false;
}
