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

#include "nrf_delay.h"
#include "lib/leds.h"
#include "lib/btn.h"

/**
 * @brief Function for application main entry.
 */
int main(void)
{
    const char* DevID = "6609";
    uint16_t pause = 500;

    leds_init();       // Configure GPIO for leds control
    btn_init();    // And for the buttons
 
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
                    while(!btn_pressed()); // Simply waiting untill the button is pressed

                    // The index i here works as an address of a particular LED in the m_led_list=LEDS_LIST
                    // array to properly form its' PIN address in the global GPIO address space
                    led_on(i); 
                    nrf_delay_ms(pause);

                    while(!btn_pressed()); // and keep it ON if the buttoin was released previously                  

                    led_off(i);
                    nrf_delay_ms(pause);                    
                }
            }
        }
    }
}

/**
 *@}
 **/