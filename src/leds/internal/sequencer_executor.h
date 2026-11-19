#ifndef _SEQUENCER_EXECUTOR_H_
#define _SEQUENCER_EXECUTOR_H_

#include <stdint.h>

#include "../leds_sequencer.h"

typedef struct {
    sequencer_state_t state;

    sequencer_config_t config;

    uint16_t step_index;

    // Start time of the current step in app_timer ticks.
    uint32_t start_time;

    // Elapsed time saved when the executor is paused.
    uint32_t elapsed_time;
} executor_t;

void executor_start(
    executor_t *executor,
    sequencer_config_t const *config);

void executor_pause(executor_t *executor);
void executor_resume(executor_t *executor);

void executor_process(
    executor_t *executor,
    sequencer_callback_t callback,
    void *context);

#endif
