#ifndef _LEDS_GPIO_H_
#define _LEDS_GPIO_H_

#include <stdint.h>

#include "pca10059.h"

void leds_gpio_on(uint32_t led_idx);
void leds_gpio_off(uint32_t led_idx);
void leds_gpio_off_all(void);
void leds_gpio_init(void);

#endif