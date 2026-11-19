#ifndef _SEQUENCER_MATH_H_
#define _SEQUENCER_MATH_H_

#include <stdint.h>

#include "../leds_sequencer.h"

uint16_t interpolate(
    uint16_t from,
    uint16_t to,
    uint32_t elapsed,
    uint32_t duration);

uint32_t get_smooth_duration(
    sequencer_step_t const *step,
    uint16_t current_value);

#endif
