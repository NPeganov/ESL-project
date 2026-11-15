#include "nrf_gpio.h"

#include "pca10059.h"

int main(void) {
    nrf_gpio_cfg_output(LED_1);
    nrf_gpio_pin_clear(LED_1);

    while (1) {}
}
