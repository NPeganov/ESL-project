#include "btn.h"

void btn_init(void)
{
    for (uint8_t btn_idx = 0; btn_idx < BUTTONS_NUMBER; ++btn_idx)
    {
        nrf_gpio_cfg_input(m_btn_list[btn_idx], BUTTON_PULL);
    }
}

bool btn_pressed(void)
{
    // Sinse there is the only button I disable the choice of the button to read state for
    uint32_t state = nrf_gpio_pin_read(m_btn_list[0]);
    return (state == BUTTONS_ACTIVE_STATE);
}