#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"
#include "nrf_drv_usbd.h"

#include "app_usbd.h"
#include "app_usbd_cdc_acm.h"

#include "usb_port.h"

#define USB_PORT_COMM_INTERFACE  2
#define USB_PORT_COMM_EPIN       NRF_DRV_USBD_EPIN3

#define USB_PORT_DATA_INTERFACE  3
#define USB_PORT_DATA_EPIN       NRF_DRV_USBD_EPIN4
#define USB_PORT_DATA_EPOUT      NRF_DRV_USBD_EPOUT4

#define USB_PORT_READ_SIZE       1
#define USB_PORT_TX_QUEUE_SIZE   64

static uint8_t m_rx_buffer[USB_PORT_READ_SIZE];
static uint8_t m_tx_buffer;
static uint8_t m_tx_queue[USB_PORT_TX_QUEUE_SIZE];
static uint8_t m_tx_queue_head;
static uint8_t m_tx_queue_tail;
static uint8_t m_tx_queue_count;
static bool m_tx_busy;
static usb_port_rx_handler_t m_rx_handler;

static void usb_event_handler(app_usbd_class_inst_t const * p_inst,
                              app_usbd_cdc_acm_user_event_t event);

APP_USBD_CDC_ACM_GLOBAL_DEF(usb_cdc_acm,
                            usb_event_handler,
                            USB_PORT_COMM_INTERFACE,
                            USB_PORT_DATA_INTERFACE,
                            USB_PORT_COMM_EPIN,
                            USB_PORT_DATA_EPIN,
                            USB_PORT_DATA_EPOUT,
                            APP_USBD_CDC_COMM_PROTOCOL_NONE);

static void tx_start(void) {
    if (m_tx_busy || m_tx_queue_count == 0)
        return;

    m_tx_buffer = m_tx_queue[m_tx_queue_tail];

    ret_code_t ret = app_usbd_cdc_acm_write(&usb_cdc_acm, &m_tx_buffer, 1);
    APP_ERROR_CHECK(ret);

    m_tx_queue_tail = (m_tx_queue_tail + 1) % USB_PORT_TX_QUEUE_SIZE;
    m_tx_queue_count--;
    m_tx_busy = true;
}

static void tx_enqueue(uint8_t byte) {
    if (m_tx_queue_count == USB_PORT_TX_QUEUE_SIZE)
        APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);

    m_tx_queue[m_tx_queue_head] = byte;
    m_tx_queue_head = (m_tx_queue_head + 1) % USB_PORT_TX_QUEUE_SIZE;
    m_tx_queue_count++;

    tx_start();
}

static void usb_event_handler(app_usbd_class_inst_t const * p_inst,
                              app_usbd_cdc_acm_user_event_t event) {
    switch (event) {
        case APP_USBD_CDC_ACM_USER_EVT_PORT_OPEN: {
            ret_code_t ret = app_usbd_cdc_acm_read(&usb_cdc_acm,
                                                   m_rx_buffer,
                                                   USB_PORT_READ_SIZE);
            if (ret != NRF_SUCCESS && ret != NRF_ERROR_IO_PENDING)
                APP_ERROR_CHECK(ret);
            break;
        }

        case APP_USBD_CDC_ACM_USER_EVT_PORT_CLOSE:
            m_tx_busy = false;
            m_tx_queue_head = 0;
            m_tx_queue_tail = 0;
            m_tx_queue_count = 0;
            break;

        case APP_USBD_CDC_ACM_USER_EVT_RX_DONE: {
            ret_code_t ret;

            do {
                m_rx_handler(m_rx_buffer[0]);

                ret = app_usbd_cdc_acm_read(&usb_cdc_acm,
                                            m_rx_buffer,
                                            USB_PORT_READ_SIZE);
            } while (ret == NRF_SUCCESS);

            if (ret != NRF_ERROR_IO_PENDING)
                APP_ERROR_CHECK(ret);
            break;
        }

        case APP_USBD_CDC_ACM_USER_EVT_TX_DONE:
            m_tx_busy = false;
            tx_start();
            break;

        default:
            break;
    }
}

bool usb_port_init(usb_port_rx_handler_t rx_handler) {
    m_rx_handler = rx_handler;

    app_usbd_class_inst_t const * class_cdc_acm =
        app_usbd_cdc_acm_class_inst_get(&usb_cdc_acm);

    ret_code_t ret = app_usbd_class_append(class_cdc_acm);
    return ret == NRF_SUCCESS;
}

void usb_port_write(uint8_t const * data, uint32_t size) {
    for (uint32_t i = 0; i < size; i++)
        tx_enqueue(data[i]);
}
