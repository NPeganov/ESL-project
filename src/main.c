/**
 * Copyright (c) 2014 - 2021, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
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

#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>

#include "nrf_delay.h"
#include "boards.h"

/**
 * @brief Function for application main entry.
 */
int main(void)
{
    const char* DevID = "6609";
    uint16_t pause = 500;

    /* Configure board. */
    bsp_board_init(BSP_INIT_LEDS);
 
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

                // It might be obvious, but who knows what a real number are hidden under BSP_BOARD_LED_X macro
                switch(i)
                {
                    case 0:led_num = BSP_BOARD_LED_0;break;
                    case 1:led_num = BSP_BOARD_LED_1;break;
                    case 2:led_num = BSP_BOARD_LED_2;break;
                    case 3:led_num = BSP_BOARD_LED_3;break;
                }

                // Blink the given number of times
                for(uint8_t j = 0; j < blink_times; j++)
                {
                    bsp_board_led_on(led_num);
                    nrf_delay_ms(pause);
                    bsp_board_led_off(led_num);
                    nrf_delay_ms(pause);                    
                }
            }
        }
    }
}

/**
 *@}
 **/
