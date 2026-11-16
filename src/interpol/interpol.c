#include <stdint.h>

#include "interpol.h"

typedef enum {
    INTERPOL_LINEAR
} interpol_curve_t;

typedef enum {
    HUE_DIR_CW,
    HUE_DIR_CCW
} hue_direction_t;

// Linear interpolation is currently the only supported curve.
static const interpol_curve_t m_curve = INTERPOL_LINEAR;

// Hue direction is reserved for the future hue-aware interpolation path.
static const hue_direction_t m_hue_direction = HUE_DIR_CW;

uint16_t interpol_linear(
    uint16_t from,
    uint16_t to,
    uint32_t elapsed,
    uint32_t duration) {
    int64_t delta;

    if (elapsed >= duration) return to;

    delta = (int64_t)to - from;

    return (uint16_t)(from + delta * elapsed / duration);
}
