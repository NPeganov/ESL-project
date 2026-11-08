
#ifndef _BTN_H_
#define _BTN_H_

#include <stdbool.h>

/**
 * @brief Button events.
 */
typedef enum
{
    BTN_EVENT_NONE,
    BTN_EVENT_PRESS,
    BTN_EVENT_RELEASE,
    BTN_EVENT_SINGLE_PRESS,
    BTN_EVENT_DOUBLE_PRESS
} btn_event_t;

/**
 * @brief Initializes button handling.
 */
void btn_init(void);

/**
 * @brief Returns the next pending button event.
 *
 * @return Next button event or BTN_EVENT_NONE.
 */
btn_event_t btn_event_take(void);

#endif