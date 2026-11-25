#ifndef _BTN_H_
#define _BTN_H_

typedef enum
{
    BTN_EVENT_NONE,          // No event.
    BTN_EVENT_PRESS,         // Confirmed physical press.
    BTN_EVENT_RELEASE,       // Confirmed physical release.
    BTN_EVENT_SINGLE_PRESS,  // Completed single press-release cycle.
    BTN_EVENT_DOUBLE_PRESS,  // Second completed cycle within the double-press interval.
    BTN_EVENT_HOLD           // Button remained pressed for the configured hold threshold.
} btn_event_t;

// Button event handler. It is called in interrupt context.
typedef void (*btn_event_handler_t)(btn_event_t event);

// Initialize button handling and enable button events.
// timer_service_init() must be called first.
void btn_init(btn_event_handler_t event_handler);

#endif
