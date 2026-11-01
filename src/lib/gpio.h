#ifndef _GPIO_H_
#define _GPIO_H_

void gpio_led_on(uint32_t led_idx);
void gpio_led_off(uint32_t led_idx);
void gpio_led_invert(uint32_t led_idx);
void gpio_leds_on(void);
void gpio_leds_off(void);
void gpio_leds_init(void);

#endif