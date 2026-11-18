#ifndef _SEQUENCER_EXECUTOR_H_
#define _SEQUENCER_EXECUTOR_H_

#include <stdbool.h>
#include <stdint.h>

#include "../leds_sequencer.h"

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

bool executor_start(
    executor_t *executor,
    sequencer_config_t const *config,
    sequencer_start_mode_t start_mode,
    uint16_t current_value,
    sequencer_callback_t callback,
    void *context);

void executor_pause(executor_t *executor);
void executor_resume(executor_t *executor);

sequencer_state_t executor_get_state(executor_t const *executor);

void executor_process(executor_t *executor);

#endif
