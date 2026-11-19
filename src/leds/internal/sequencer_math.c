#include <stdint.h>

#include "sequencer_math.h"

uint16_t interpolate(
    uint16_t from,
    uint16_t to,
    uint32_t elapsed,
    uint32_t duration) {
    int64_t delta;

    if (duration == 0 || elapsed >= duration) return to;

    delta = (int64_t)to - from;

    return (uint16_t)(from + delta * elapsed / duration);
}

uint32_t get_smooth_duration(
    uint16_t from,
    uint16_t to,
    uint32_t duration_ms,
    uint16_t current_value) {
    uint32_t original_distance = from > to ?
        from - to : to - from;
    uint32_t remaining_distance = current_value > to ?
        current_value - to : to - current_value;

    if (original_distance == 0)
        return duration_ms;

    return (uint32_t)(
        (uint64_t)duration_ms * remaining_distance /
        original_distance);
}
