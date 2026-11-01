#include "pca10059.h"

#include "gpio.h"

void gpio_led_on(uint32_t led_idx)
{
    ASSERT(led_idx < LEDS_NUMBER);
    nrf_gpio_pin_write(m_led_list[led_idx], LEDS_ACTIVE_STATE);
}

void gpio_led_off(uint32_t led_idx)
{
    ASSERT(led_idx < LEDS_NUMBER);
    nrf_gpio_pin_write(m_led_list[led_idx], !LEDS_ACTIVE_STATE);
}

void gpio_led_invert(uint32_t led_idx)
{
    ASSERT(led_idx < LEDS_NUMBER);
    nrf_gpio_pin_toggle(m_led_list[led_idx]);
}

void gpio_leds_on(void)
{
    for (size_t led_idx = 0; led_idx < LEDS_NUMBER; led_idx++)
    {
        gpio_led_on(led_idx);
    }
}

void gpio_leds_off(void)
{
    for (size_t led_idx = 0; led_idx < LEDS_NUMBER; led_idx++)
    {
        gpio_led_off(led_idx);
    }
}

void gpio_leds_init(void)
{
    for (uint8_t led_idx = 0; led_idx < LEDS_NUMBER; ++led_idx)
    {
        nrf_gpio_cfg_output(m_led_list[led_idx]);
    }
    gpio_leds_off();
}

bool gpio_button_state(void)
{
    // Sinse there is the only button I disable the choice of the button to read state for
    uint32_t state = nrf_gpio_pin_read(m_btn_list[0]);
    return (state == BUTTONS_ACTIVE_STATE);
}

void gpio_buttons_init(void)
{
    for (uint8_t btn_idx = 0; btn_idx < BUTTONS_NUMBER; ++btn_idx)
    {
        nrf_gpio_cfg_input(m_btn_list[btn_idx], BUTTON_PULL);
    }
}