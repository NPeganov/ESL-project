#include <stdint.h>

#include "sequencer_math.h"

uint16_t sequencer_math_interpolate(
    uint16_t from,
    uint16_t to,
    uint32_t elapsed,
    uint32_t duration) {
    int64_t delta;

    if (duration == 0 || elapsed >= duration) return to;

    delta = (int64_t)to - from;

    return (uint16_t)(from + delta * elapsed / duration);
}

uint32_t sequencer_math_smooth_duration_get(
    sequencer_step_t const *step,
    uint16_t current_value) {
    uint32_t original_distance = step->from > step->to ?
        step->from - step->to : step->to - step->from;
    uint32_t remaining_distance = current_value > step->to ?
        current_value - step->to : step->to - current_value;

    if (original_distance == 0)
        return step->duration_ms;

    return (uint32_t)(
        (uint64_t)step->duration_ms * remaining_distance /
        original_distance);
}
