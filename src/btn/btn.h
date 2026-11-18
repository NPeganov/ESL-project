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

// Button event handler. It is called in interrupt context.
typedef void (*btn_event_handler_t)(btn_event_t event);

// Configure button handling and register its event handler.
// timer_service_init() must be called first.
void btn_init(btn_event_handler_t event_handler);

// Enable GPIO events and start initial button-state detection.
void btn_start(void);

#endif
