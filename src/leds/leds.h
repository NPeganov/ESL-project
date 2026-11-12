#ifndef _LEDS_H_
#define _LEDS_H_

#include <stdint.h>

#include "pca10059.h"

void led_on(uint32_t led_idx);
void led_off(uint32_t led_idx);
void leds_off(void);
void leds_init(void);

#endif