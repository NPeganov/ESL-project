#include <stddef.h>
#include <stdint.h>

#include "app_timer.h"

#include "interpol.h"
#include "sequencer_executor.h"

static bool executor_advance(executor_t *executor);
static void executor_clear(executor_t *executor);

void executor_start(
    executor_t *executor,
    sequencer_config_t const *config,
    sequencer_callback_t callback,
    void *context) {
    executor->config = *config;
    executor->callback = callback;
    executor->context = context;
    executor->step_index = 0;
    executor->start_time = app_timer_cnt_get();
    executor->elapsed_time = 0;
    executor->state = SEQUENCER_STATE_RUNNING;
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

        if (duration == 0 || elapsed >= duration) {
            executor->callback(step->to, executor->context);
            executor->start_time += duration;

            if (!executor_advance(executor))
                return;

            continue;
        }

        executor->callback(
            interpol_linear(
                step->from,
                step->to,
                elapsed,
                duration),
            executor->context);

        return;
    }
}

static bool executor_advance(executor_t *executor) {
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
    executor->config.steps = NULL;
    executor->callback = NULL;
    executor->context = NULL;
}
