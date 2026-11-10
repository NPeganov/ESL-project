#ifndef _BTN_H_
#define _BTN_H_

// Button events:
// - PRESS and RELEASE report confirmed physical state changes.
// - SINGLE_PRESS is generated after every completed press-release cycle.
// - DOUBLE_PRESS is additionally generated when the second completed
//   cycle occurs within the configured double-press interval.
typedef enum
{
    BTN_EVENT_NONE,
    BTN_EVENT_PRESS,
    BTN_EVENT_RELEASE,
    BTN_EVENT_SINGLE_PRESS,
    BTN_EVENT_DOUBLE_PRESS
} btn_event_t;

// Initializes the clock, timer subsystem and button handling.
void btn_init(void);

// Returns the next pending event, or BTN_EVENT_NONE if the queue is empty.
btn_event_t btn_event_take(void);

#endif