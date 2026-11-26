#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"
#include "nrf.h"
#include "nrf_drv_usbd.h"

#include "app_usbd.h"
#include "app_usbd_cdc_acm.h"

#include "hsv_picker/hsv_picker.h"
#include "log/log.h"

#define CDC_ACM_COMM_INTERFACE  2
#define CDC_ACM_COMM_EPIN       NRF_DRV_USBD_EPIN3

#define CDC_ACM_DATA_INTERFACE  3
#define CDC_ACM_DATA_EPIN       NRF_DRV_USBD_EPIN4
#define CDC_ACM_DATA_EPOUT      NRF_DRV_USBD_EPOUT4

#define CDC_ACM_READ_SIZE       1

static uint8_t m_rx_buffer[CDC_ACM_READ_SIZE];

static void usb_event_handler(app_usbd_class_inst_t const * p_inst,
                              app_usbd_cdc_acm_user_event_t event);

APP_USBD_CDC_ACM_GLOBAL_DEF(usb_cdc_acm,
                            usb_event_handler,
                            CDC_ACM_COMM_INTERFACE,
                            CDC_ACM_DATA_INTERFACE,
                            CDC_ACM_COMM_EPIN,
                            CDC_ACM_DATA_EPIN,
                            CDC_ACM_DATA_EPOUT,
                            APP_USBD_CDC_COMM_PROTOCOL_NONE);

static void usb_event_handler(app_usbd_class_inst_t const * p_inst,
                              app_usbd_cdc_acm_user_event_t event) {
    switch (event) {
        case APP_USBD_CDC_ACM_USER_EVT_PORT_OPEN: {
            ret_code_t ret = app_usbd_cdc_acm_read(&usb_cdc_acm,
                                                   m_rx_buffer,
                                                   CDC_ACM_READ_SIZE);
            APP_ERROR_CHECK(ret);
            break;
        }

        case APP_USBD_CDC_ACM_USER_EVT_RX_DONE: {
            ret_code_t ret;

            do {
                if (m_rx_buffer[0] == '\r' || m_rx_buffer[0] == '\n')
                    ret = app_usbd_cdc_acm_write(&usb_cdc_acm, "\r\n", 2);
                else
                    ret = app_usbd_cdc_acm_write(&usb_cdc_acm,
                                                 m_rx_buffer,
                                                 CDC_ACM_READ_SIZE);

                ret = app_usbd_cdc_acm_read(&usb_cdc_acm,
                                            m_rx_buffer,
                                            CDC_ACM_READ_SIZE);
            } while (ret == NRF_SUCCESS);

            break;
        }

        default:
            break;
    }
}

int main(void) {
    log_init();

    app_usbd_class_inst_t const * class_cdc_acm =
        app_usbd_cdc_acm_class_inst_get(&usb_cdc_acm);
    APP_ERROR_CHECK(app_usbd_class_append(class_cdc_acm));

    // Initialize the HSV Picker and prepare its initial indication.
    if (!hsv_picker_init())
        APP_ERROR_HANDLER(NRF_ERROR_INTERNAL);

    // Enable user interaction.
    hsv_picker_start();

    // Process deferred flash writes and log messages, then sleep until an event.
    while (true) {
        hsv_picker_process();
        log_process();
        __WFE();
    }
}
