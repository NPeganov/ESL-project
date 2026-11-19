#ifndef _SEQUENCER_MATH_H_
#define _SEQUENCER_MATH_H_

#include <stdint.h>

uint16_t interpolate(
    uint16_t from,
    uint16_t to,
    uint32_t elapsed,
    uint32_t duration);

uint32_t get_smooth_duration(
    uint16_t from,
    uint16_t to,
    uint32_t duration_ms,
    uint16_t current_value);

#endif
