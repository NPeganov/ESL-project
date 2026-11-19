#ifndef _INTERPOL_H_
#define _INTERPOL_H_

#include <stdint.h>

uint16_t interpol_linear(
    uint16_t from,
    uint16_t to,
    uint32_t elapsed,
    uint32_t duration);

#endif
