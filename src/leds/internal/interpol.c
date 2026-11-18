#include <stdint.h>

#include "interpol.h"

typedef enum {
    HUE_DIR_CW,
    HUE_DIR_CCW
} hue_direction_t;

typedef enum {
    HSV_VALUE_DIR_UP,
    HSV_VALUE_DIR_DOWN
} hsv_value_direction_t;

// Hue follows one fixed direction; S and V share one initial direction.
#define HUE_DIRECTION HUE_DIR_CW
#define HSV_VALUE_DIRECTION HSV_VALUE_DIR_UP

static uint16_t interpol_linear(
    uint16_t from,
    uint16_t to,
    uint32_t elapsed,
    uint32_t duration) {
    int64_t delta;

    if (duration == 0 || elapsed >= duration) return to;

    delta = (int64_t)to - from;

    return (uint16_t)(from + delta * elapsed / duration);
}

static uint16_t interpol_hue(
    uint16_t from,
    uint16_t start_value,
    uint32_t elapsed,
    uint32_t duration) {
    uint16_t distance;
    uint32_t progress;

    if (duration == 0) return from;

    if (HUE_DIRECTION == HUE_DIR_CW)
        distance = (from + 360 - start_value) % 360;
    else
        distance = (start_value + 360 - from) % 360;

    if (distance == 0)
        distance = 360;

    if (elapsed >= duration) return from;

    progress = (uint64_t)distance * elapsed / duration;

    if (HUE_DIRECTION == HUE_DIR_CW)
        return (start_value + progress) % 360;

    return (start_value + 360 - progress) % 360;
}

static uint16_t interpol_bounce(
    uint16_t from,
    uint16_t to,
    uint16_t start_value,
    uint32_t elapsed,
    uint32_t duration) {
    uint16_t range = to - from;
    uint64_t period;
    uint64_t phase;
    uint64_t position;

    if (duration == 0 || range == 0) return start_value;

    period = (uint64_t)duration * 2;
    position = (uint64_t)(start_value - from) * duration / range;

    if (HSV_VALUE_DIRECTION == HSV_VALUE_DIR_UP)
        phase = position + elapsed;
    else
        phase = period - position + elapsed;

    phase %= period;

    if (phase > duration)
        phase = period - phase;

    return from + (uint16_t)(phase * range / duration);
}

uint16_t interpol_value(
    seq_scale_t scale,
    uint16_t from,
    uint16_t to,
    uint16_t start_value,
    uint32_t elapsed,
    uint32_t duration) {
    switch (scale) {
        case SEQ_SCALE_CYCLIC:
            return interpol_hue(from, start_value, elapsed, duration);

        case SEQ_SCALE_BOUNCE:
            return interpol_bounce(
                from, to, start_value, elapsed, duration);

        case SEQ_SCALE_LINEAR:
        default:
            return interpol_linear(start_value, to, elapsed, duration);
    }
}
