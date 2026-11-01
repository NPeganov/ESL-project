/** @file
 *
 * @defgroup blinky_example_main main.c
 * @{
 * @ingroup blinky_example
 * @brief Blinky Example Application main file.
 *
 * This file contains the source code for a sample application to blink LEDs.
 *
 */

#include <ctype.h>

#include "pca10059.h"
#include "nrf_delay.h"
#include "lib/gpio.h"

/**
 * @brief Function for application main entry.
 */
int main(void)
{
    const char* DevID = "6609";
    uint16_t pause = 500;

    /* Configure leds. */
    gpio_leds_init();
 
    while (true)
    {
        for (uint8_t i = 0; i < LEDS_NUMBER; i++)
        {   
            // This loop iterates on each character within device ID.
            // On every iteration a character is checed whether it is a digit or not.
            // In the case it is it is being conveted into an integer = number of times to blink.
            if((bool)isdigit((uint8_t)*(DevID + i)))
            {
                uint8_t blink_times = *(DevID + i) - 48;
                uint8_t led_num = 0;

                // It might be obvious, but who knows what a real number are hidden under BSP_LED_X macro
                switch(i)
                {
                    case 0:led_num = 0;break;
                    case 1:led_num = 1;break;
                    case 2:led_num = 2;break;
                    case 3:led_num = 3;break;
                }

                // Blink the given number of times
                for(uint8_t j = 0; j < blink_times; j++)
                {
                    gpio_led_on(led_num);
                    nrf_delay_ms(pause);
                    gpio_led_off(led_num);
                    nrf_delay_ms(pause);                    
                }
            }
        }
    }
}

/**
 *@}
 **/