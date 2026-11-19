#include <stddef.h>
#include <stdint.h>

#include "app_timer.h"

#include "interpol.h"
#include "sequencer_executor.h"

static bool config_is_valid(sequencer_config_t const *config);
static bool config_has_timed_step(sequencer_config_t const *config);
static bool executor_advance(executor_t *executor);
static void executor_clear(executor_t *executor);

bool executor_start(
    executor_t *executor,
    sequencer_config_t const *config,
    sequencer_start_mode_t start_mode,
    uint16_t current_value,
    sequencer_callback_t callback,
    void *context) {
    if (executor == NULL || !config_is_valid(config) || callback == NULL)
        return false;

    if (start_mode != SEQUENCER_START_FROM_STEP &&
        start_mode != SEQUENCER_START_CURRENT)
        return false;

    if (config->repeat && !config_has_timed_step(config))
        return false;

    executor->config = *config;
    executor->callback = callback;
    executor->context = context;
    executor->step_index = 0;
    executor->start_value = current_value;
    executor->start_value_pending = (start_mode == SEQUENCER_START_CURRENT);
    executor->start_time = app_timer_cnt_get();
    executor->elapsed_time = 0;
    executor->state = SEQUENCER_STATE_RUNNING;

    executor_process(executor);

    return true;
}

void executor_pause(executor_t *executor) {
    if (executor == NULL) return;

    if (executor->state == SEQUENCER_STATE_RUNNING) {
        executor->elapsed_time = app_timer_cnt_diff_compute(
            app_timer_cnt_get(),
            executor->start_time);

        executor->state = SEQUENCER_STATE_PAUSED;
    }
}

void executor_resume(executor_t *executor) {
    if (executor == NULL) return;

    if (executor->state == SEQUENCER_STATE_PAUSED) {
        executor->start_time = app_timer_cnt_get() - executor->elapsed_time;
        executor->state = SEQUENCER_STATE_RUNNING;
    }
}

sequencer_state_t executor_get_state(executor_t const *executor) {
    if (executor == NULL) return SEQUENCER_STATE_IDLE;

    return executor->state;
}

void executor_process(executor_t *executor) {
    uint32_t now;

    if (executor->state != SEQUENCER_STATE_RUNNING) return;

    now = app_timer_cnt_get();

    while (executor->state == SEQUENCER_STATE_RUNNING) {
        sequencer_step_t const *step = &executor->config.steps[executor->step_index];
        uint32_t duration = APP_TIMER_TICKS(step->duration_ms);
        uint32_t elapsed = app_timer_cnt_diff_compute(now, executor->start_time);
        uint16_t start_value = executor->start_value_pending ?
            executor->start_value : step->from;

        if (duration == 0) {
            executor->callback(step->to, executor->context);

            if (!executor_advance(executor))
                return;

            continue;
        }

        if (elapsed >= duration) {
            executor->callback(step->to, executor->context);
            executor->start_time += duration;

            if (!executor_advance(executor))
                return;

            continue;
        }

        executor->callback(
            interpol_linear(
                start_value,
                step->to,
                elapsed,
                duration),
            executor->context);

        return;
    }
}

static bool config_is_valid(sequencer_config_t const *config) {
    if (config == NULL) return false;
    if (config->steps == NULL) return false;
    if (config->step_count == 0) return false;

    return true;
}

static bool config_has_timed_step(sequencer_config_t const *config) {
    for (uint16_t i = 0; i < config->step_count; i++)
        if (config->steps[i].duration_ms > 0) return true;

    return false;
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
}
