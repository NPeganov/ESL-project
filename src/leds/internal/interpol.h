#ifndef _INTERPOL_H_
#define _INTERPOL_H_

#include <stdint.h>

#include "../leds_seq.h"

uint16_t interpol_value(
    seq_scale_t scale,
    uint16_t from,
    uint16_t to,
    uint16_t start_value,
    uint32_t elapsed,
    uint32_t duration);

#endif
