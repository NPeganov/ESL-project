#include <stdint.h>

#include "nrf_assert.h"
#include "nrf_gpio.h"

#include "leds_gpio.h"

// Map logical LED indices to board GPIO pins.
static const uint8_t m_led_list[LEDS_NUMBER] = LEDS_LIST;

void leds_gpio_on(uint32_t led_idx) {
    ASSERT(led_idx < LEDS_NUMBER);
    nrf_gpio_pin_write(m_led_list[led_idx], LEDS_ACTIVE_STATE);
}

void leds_gpio_off(uint32_t led_idx) {
    ASSERT(led_idx < LEDS_NUMBER);
    nrf_gpio_pin_write(m_led_list[led_idx], !LEDS_ACTIVE_STATE);
}

void leds_gpio_off_all(void) {
    for (uint32_t led_idx = 0; led_idx < LEDS_NUMBER; led_idx++)
        leds_gpio_off(led_idx);
}

void leds_gpio_init(void) {
    for (uint32_t led_idx = 0; led_idx < LEDS_NUMBER; led_idx++)
        nrf_gpio_cfg_output(m_led_list[led_idx]);

    // Initialize all LEDs in the off state.
    leds_gpio_off_all();
}