#ifndef _GPIO_H_
#define _GPIO_H_

static const uint8_t m_led_list[LEDS_NUMBER] = LEDS_LIST;

void gpio_led_on(uint32_t led_idx);
void gpio_led_off(uint32_t led_idx);
void gpio_led_invert(uint32_t led_idx);
void gpio_leds_on(void);
void gpio_leds_off(void);
void gpio_leds_init(void);

static const uint8_t m_btn_list[BUTTONS_NUMBER] = BUTTONS_LIST;

bool gpio_button_state(void);
void gpio_buttons_init(void);

#endif