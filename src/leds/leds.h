#ifndef _LEDS_H_
#define _LEDS_H_

#include "pca10059.h"

static const uint8_t m_led_list[LEDS_NUMBER] = LEDS_LIST;

void led_on(uint32_t led_idx);
void led_off(uint32_t led_idx);
void leds_off(void);
void leds_init(void);

#endif