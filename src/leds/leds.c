#include "leds.h"

void led_on(uint32_t led_idx)
{
    ASSERT(led_idx < LEDS_NUMBER);
    nrf_gpio_pin_write(m_led_list[led_idx], LEDS_ACTIVE_STATE);
}

void led_off(uint32_t led_idx)
{
    ASSERT(led_idx < LEDS_NUMBER);
    nrf_gpio_pin_write(m_led_list[led_idx], !LEDS_ACTIVE_STATE);
}

void leds_off(void)
{
    for (size_t led_idx = 0; led_idx < LEDS_NUMBER; led_idx++)
    {
        led_off(led_idx);
    }
}

void leds_init(void)
{
    for (uint8_t led_idx = 0; led_idx < LEDS_NUMBER; ++led_idx)
    {
        nrf_gpio_cfg_output(m_led_list[led_idx]);
    }
    leds_off();
}