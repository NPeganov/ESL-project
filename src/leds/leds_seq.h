#ifndef _SEQ_H_
#define _SEQ_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    YELLOW_LED,
    RGB_LED
} seq_led_id;

typedef enum {
    SEQ_START_FROM_STEP,
    SEQ_START_CURRENT
} seq_start_mode_t;

typedef enum {
    SEQ_STATE_IDLE,
    SEQ_STATE_RUNNING,
    SEQ_STATE_PAUSED
} seq_state_t;

typedef enum {
    SEQ_SCALE_LINEAR,
    SEQ_SCALE_CYCLIC, // One full cycle over the 0..360 range.
    SEQ_SCALE_BOUNCE  // Oscillation between the step boundaries.
} seq_scale_t;

typedef struct {
    uint16_t from; // Start value or lower boundary, depending on scale.
    uint16_t to; // Target value or upper boundary, depending on scale.
    uint32_t duration_ms;
} seq_step_t;

typedef struct {
    seq_step_t const *steps;
    uint16_t step_count;
    bool repeat;
    seq_scale_t scale; // Interpolation scale used by every step.
} seq_config_t;

typedef void (*seq_callback_t)(uint16_t value, void *context);

bool seq_init(void);

bool seq_start(
    seq_led_id id,
    seq_config_t const *config,
    seq_start_mode_t start_mode,
    uint16_t current_value,
    seq_callback_t callback,
    void *context);

void seq_pause(seq_led_id id);
void seq_resume(seq_led_id id);

seq_state_t seq_get_state(seq_led_id id);

#endif
