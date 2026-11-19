#include <stdbool.h>
#include <stdint.h>

#include "nrf_delay.h"

#include "leds/leds_pwm.h"

#define DEMO_PERIOD_MS 1000
#define RGB_COLOR_COUNT 6

static const uint8_t m_rgb_colors[RGB_COLOR_COUNT][3] = {
    { 255, 0, 0 },       // Red
    { 0, 255, 0 },       // Green
    { 0, 0, 255 },       // Blue
    { 255, 255, 0 },     // Red + green
    { 0, 255, 255 },     // Green + blue
    { 255, 0, 255 }      // Red + blue
};

int main(void) {
    uint8_t color = 0;
    bool yellow_bright = true;

    if (!leds_pwm_init())
        while (true);

    while (true) {
        leds_pwm_set_yellow_brightness(yellow_bright ? 100 : 50);
        leds_pwm_set_rgb(
            m_rgb_colors[color][0],
            m_rgb_colors[color][1],
            m_rgb_colors[color][2]);

        yellow_bright = !yellow_bright;
        color = (color + 1) % RGB_COLOR_COUNT;

        nrf_delay_ms(DEMO_PERIOD_MS);
    }
}
