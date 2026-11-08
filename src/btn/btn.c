#include <stdbool.h>
#include <stdint.h>

#include "app_error.h"
#include "app_timer.h"
#include "app_util_platform.h"

#include "btn.h"
#include "btn_hw.h"

#ifndef BTN_DOUBLE_PRESS_TIME_MS
#define BTN_DOUBLE_PRESS_TIME_MS 1000
#endif

#ifndef BTN_EVENT_QUEUE_SIZE
#define BTN_EVENT_QUEUE_SIZE 8
#endif

APP_TIMER_DEF(m_double_press_timer);

static btn_event_t m_event_queue[BTN_EVENT_QUEUE_SIZE];

static volatile uint8_t m_event_head;
static volatile uint8_t m_event_tail;
static volatile uint8_t m_event_count;

static bool m_press_started;
static bool m_double_press_pending;

static uint32_t m_first_release_tick;

/**
 * @brief Adds an event to the button event queue.
 *
 * @param[in] event Event to add.
 */
static void event_queue_push(btn_event_t event);

/**
 * @brief Handles confirmed button state changes.
 *
 * @param[in] pressed True if the button is pressed.
 */
static void button_state_changed(bool pressed);

/**
 * @brief Handles double-press timer expiration.
 *
 * @param[in] p_context Timer context.
 */
static void double_press_timer_handler(void *p_context);

void btn_init(void)
{
    APP_ERROR_CHECK(app_timer_init());

    APP_ERROR_CHECK(app_timer_create(
        &m_double_press_timer,
        APP_TIMER_MODE_SINGLE_SHOT,
        double_press_timer_handler));

    btn_hw_init(button_state_changed);
}

btn_event_t btn_event_take(void)
{
    btn_event_t event = BTN_EVENT_NONE;

    CRITICAL_REGION_ENTER();

    if (m_event_count > 0)
    {
        event = m_event_queue[m_event_tail];

        m_event_tail++;
        if (m_event_tail >= BTN_EVENT_QUEUE_SIZE)
            m_event_tail = 0;

        m_event_count--;
    }

    CRITICAL_REGION_EXIT();

    return event;
}

static void event_queue_push(btn_event_t event)
{
    CRITICAL_REGION_ENTER();

    if (m_event_count < BTN_EVENT_QUEUE_SIZE)
    {
        m_event_queue[m_event_head] = event;

        m_event_head++;
        if (m_event_head >= BTN_EVENT_QUEUE_SIZE)
            m_event_head = 0;

        m_event_count++;
    }

    CRITICAL_REGION_EXIT();
}

static void button_state_changed(bool pressed)
{
    uint32_t now;

    if (pressed)
    {
        m_press_started = true;
        event_queue_push(BTN_EVENT_PRESS);
        return;
    }

    event_queue_push(BTN_EVENT_RELEASE);

    if (!m_press_started) return;

    m_press_started = false;

    event_queue_push(BTN_EVENT_SINGLE_PRESS);

    now = app_timer_cnt_get();

    if (m_double_press_pending)
    {
        if (app_timer_cnt_diff_compute(now, m_first_release_tick) <
            APP_TIMER_TICKS(BTN_DOUBLE_PRESS_TIME_MS))
        {
            m_double_press_pending = false;

            APP_ERROR_CHECK(app_timer_stop(m_double_press_timer));

            event_queue_push(BTN_EVENT_DOUBLE_PRESS);
            return;
        }

        APP_ERROR_CHECK(app_timer_stop(m_double_press_timer));
    }

    m_double_press_pending = true;
    m_first_release_tick = now;

    APP_ERROR_CHECK(app_timer_start(
        m_double_press_timer,
        APP_TIMER_TICKS(BTN_DOUBLE_PRESS_TIME_MS),
        NULL));
}

static void double_press_timer_handler(void *p_context)
{
    (void)p_context;

    m_double_press_pending = false;
}