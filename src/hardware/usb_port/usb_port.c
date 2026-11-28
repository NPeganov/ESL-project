#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "app_error.h"
#include "nrf_drv_usbd.h"

#include "app_usbd.h"
#include "app_usbd_cdc_acm.h"

#include "usb_port.h"

// Interfaces and endpoints are reserved for this CDC ACM instance.
#define USB_PORT_COMM_INTERFACE  2
#define USB_PORT_COMM_EPIN       NRF_DRV_USBD_EPIN3

#define USB_PORT_DATA_INTERFACE  3
#define USB_PORT_DATA_EPIN       NRF_DRV_USBD_EPIN4
#define USB_PORT_DATA_EPOUT      NRF_DRV_USBD_EPOUT4

#define USB_PORT_RX_BUFFER_SIZE  64
#define USB_PORT_TX_BUFFER_SIZE  64
#define USB_PORT_TX_QUEUE_SIZE   64

static uint8_t m_rx_buffer[USB_PORT_RX_BUFFER_SIZE];
static uint8_t m_tx_buffer[USB_PORT_TX_BUFFER_SIZE];
static uint8_t m_tx_queue[USB_PORT_TX_QUEUE_SIZE];
static size_t m_tx_queue_head;
static size_t m_tx_queue_tail;
static size_t m_tx_queue_count;
static bool m_initialized;
static bool m_port_open;
static bool m_tx_busy;
static usb_port_rx_handler_t m_rx_handler;
static usb_port_state_handler_t m_state_handler;

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
    if (!m_port_open || m_tx_busy || m_tx_queue_count == 0)
        return;

    size_t chunk_size = USB_PORT_TX_QUEUE_SIZE - m_tx_queue_tail;
    if (chunk_size > m_tx_queue_count)
        chunk_size = m_tx_queue_count;
    if (chunk_size > USB_PORT_TX_BUFFER_SIZE)
        chunk_size = USB_PORT_TX_BUFFER_SIZE;

    memcpy(m_tx_buffer, &m_tx_queue[m_tx_queue_tail], chunk_size);

    ret_code_t ret = app_usbd_cdc_acm_write(&usb_cdc_acm, m_tx_buffer, chunk_size);
    // The host may have reset DTR before the next PORT_OPEN event is processed.
    if (ret == NRF_ERROR_INVALID_STATE)
        return;

    APP_ERROR_CHECK(ret);

    // Queue space is released once the data has been copied to the active TX buffer.
    m_tx_queue_tail = (m_tx_queue_tail + chunk_size) % USB_PORT_TX_QUEUE_SIZE;
    m_tx_queue_count -= chunk_size;
    m_tx_busy = true;
}

static void rx_read(void) {
    ret_code_t ret;

    do {
        ret = app_usbd_cdc_acm_read_any(&usb_cdc_acm,
                                        m_rx_buffer,
                                        sizeof(m_rx_buffer));

        if (ret == NRF_SUCCESS) {
            size_t size = app_usbd_cdc_acm_rx_size(&usb_cdc_acm);
            if (size > 0)
                m_rx_handler(m_rx_buffer, size);
        }
    } while (ret == NRF_SUCCESS);

    if (ret != NRF_ERROR_IO_PENDING)
        APP_ERROR_CHECK(ret);
}

static void tx_state_reset(void) {
    m_tx_queue_head = 0;
    m_tx_queue_tail = 0;
    m_tx_queue_count = 0;
    m_tx_busy = false;
}

static void usb_event_handler(app_usbd_class_inst_t const *,
                              app_usbd_cdc_acm_user_event_t event) {
    switch (event) {
        case APP_USBD_CDC_ACM_USER_EVT_PORT_OPEN:
            // A bus reset clears DTR without sending PORT_CLOSE.
            // The host's next DTR assertion generates another PORT_OPEN.
            if (m_port_open) {
                m_port_open = false;
                tx_state_reset();
                if (m_state_handler != NULL)
                    m_state_handler(false);
            }

            m_port_open = true;
            if (m_state_handler != NULL)
                m_state_handler(true);

            rx_read();
            tx_start();
            break;

        case APP_USBD_CDC_ACM_USER_EVT_PORT_CLOSE:
            if (m_port_open) {
                m_port_open = false;
                tx_state_reset();
                if (m_state_handler != NULL)
                    m_state_handler(false);
            }
            else
                tx_state_reset();
            break;

        case APP_USBD_CDC_ACM_USER_EVT_RX_DONE: {
            size_t size = app_usbd_cdc_acm_rx_size(&usb_cdc_acm);
            if (size > 0)
                m_rx_handler(m_rx_buffer, size);
            rx_read();
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

bool usb_port_init(usb_port_rx_handler_t rx_handler,
                   usb_port_state_handler_t state_handler) {
    if (m_initialized || rx_handler == NULL)
        return false;

    m_rx_handler = rx_handler;
    m_state_handler = state_handler;

    app_usbd_class_inst_t const * class_cdc_acm =
        app_usbd_cdc_acm_class_inst_get(&usb_cdc_acm);

    ret_code_t ret = app_usbd_class_append(class_cdc_acm);
    if (ret != NRF_SUCCESS)
        return false;

    m_initialized = true;
    return true;
}

bool usb_port_write(uint8_t const * data, size_t size) {
    if (!m_initialized || !m_port_open || data == NULL || size == 0 ||
        size > USB_PORT_TX_QUEUE_SIZE - m_tx_queue_count)
        return false;

    for (size_t i = 0; i < size; i++) {
        m_tx_queue[m_tx_queue_head] = data[i];
        m_tx_queue_head = (m_tx_queue_head + 1) % USB_PORT_TX_QUEUE_SIZE;
    }

    m_tx_queue_count += size;
    tx_start();

    return true;
}
