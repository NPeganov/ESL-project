#ifndef _LEDS_SEQUENCER_H_
#define _LEDS_SEQUENCER_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SEQUENCER_YELLOW_LED,
    SEQUENCER_RGB_LED
} sequencer_led_id_t;

typedef enum {
    SEQUENCER_START_FROM_STEP,
    SEQUENCER_START_CURRENT
} sequencer_start_mode_t;

typedef enum {
    SEQUENCER_STATE_IDLE,
    SEQUENCER_STATE_RUNNING,
    SEQUENCER_STATE_PAUSED
} sequencer_state_t;

typedef enum {
    SEQUENCER_SCALE_LINEAR,
    SEQUENCER_SCALE_CYCLIC, // One full cycle over the 0..360 range.
    SEQUENCER_SCALE_BOUNCE  // Oscillation between the step boundaries.
} sequencer_scale_t;

typedef struct {
    uint16_t from; // Start value or lower boundary, depending on scale.
    uint16_t to; // Target value or upper boundary, depending on scale.
    uint32_t duration_ms;
} sequencer_step_t;

typedef struct {
    sequencer_step_t const *steps;
    uint16_t step_count;
    bool repeat;
    sequencer_scale_t scale; // Interpolation scale used by every step.
} sequencer_config_t;

typedef void (*sequencer_callback_t)(uint16_t value, void *context);

bool sequencer_init(void);

bool sequencer_start(
    sequencer_led_id_t id,
    sequencer_config_t const *config,
    sequencer_start_mode_t start_mode,
    uint16_t current_value,
    sequencer_callback_t callback,
    void *context);

void sequencer_pause(sequencer_led_id_t id);
void sequencer_resume(sequencer_led_id_t id);

sequencer_state_t sequencer_get_state(sequencer_led_id_t id);

#endif
