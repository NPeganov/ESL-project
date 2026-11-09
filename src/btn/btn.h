#ifndef _BTN_H_
#define _BTN_H_

#include <stdbool.h>

/**
 * @brief Button events.
 *
 * BTN_EVENT_PRESS and BTN_EVENT_RELEASE report confirmed
 * physical state changes after hardware debounce.
 *
 * BTN_EVENT_SINGLE_PRESS is generated after every completed
 * press-release cycle.
 *
 * BTN_EVENT_DOUBLE_PRESS is generated additionally when the
 * second completed press-release cycle occurs within the
 * configured double-press interval.
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
 *
 * Initializes the logical button layer and its hardware layer.
 */
void btn_init(void);

/**
 * @brief Returns the next pending button event.
 *
 * Events are returned in the same order in which they were
 * generated. If no events are pending, BTN_EVENT_NONE is returned.
 *
 * @return Next button event or BTN_EVENT_NONE.
 */
btn_event_t btn_event_take(void);

#endif