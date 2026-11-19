#include <stdbool.h>

#include "nrf.h"

#include "btn/btn.h"
#include "hsv_picker/leds_control/leds_control.h"
#include "log/log.h"

int main(void) {
    btn_init();
    log_init();

    if (!lc_init())
        while (true);

    lc_set_hsv(120, 100, 100);
    lc_set_yellow_pattern(LC_YELLOW_SLOW_BLINK);

    while (true) {
        log_process();
        __WFE();
    }
}
