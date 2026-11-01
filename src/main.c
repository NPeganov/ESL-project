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

    gpio_leds_init();       // Configure GPIO for leds control
    gpio_buttons_init();    // And for the buttons
 
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

                // Blink the given number of times
                for(uint8_t j = 0; j < blink_times; j++)
                {
                    while(!gpio_button_state()); // Simply waiting untill the button is pressed

                    // The index i here works as an address of a particular LED in the m_led_list=LEDS_LIST
                    // array to properly form its' PIN address in the global GPIO address space
                    gpio_led_on(i); 
                    nrf_delay_ms(pause);

                    while(!gpio_button_state()); // and keep it ON if the buttoin was released previously                  

                    gpio_led_off(i);
                    nrf_delay_ms(pause);                    
                }
            }
        }
    }
}

/**
 *@}
 **/