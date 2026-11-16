# PWM LED module

## Purpose

`leds_pwm` is the high-level LED abstraction for the board's PWM-controlled yellow and RGB LEDs.

It is intentionally independent of the application modules that may use it. It can be initialized and used directly by an application without the button, fade, sequence, or controller modules.

The module hides:

- PWM channel assignment;
- board GPIO pins;
- active-low polarity;
- PWM frequency;
- conversion between HSV and RGB;
- conversion from percentage values to PWM duty.

Its dependency is the universal hardware `pwm` module.

~~~ text
application
    |
    v
leds_pwm
    |
    v
pwm
    |
    v
nRF52840 PWM
~~~

## Public interface

~~~ c
bool leds_pwm_init(void);

void leds_pwm_set_rgb(uint8_t red, uint8_t green, uint8_t blue);
void leds_pwm_set_hsv(uint16_t hue, uint8_t saturation, uint8_t value);

void leds_pwm_get_rgb(uint8_t *red, uint8_t *green, uint8_t *blue);
void leds_pwm_get_hsv(uint16_t *hue, uint8_t *saturation, uint8_t *value);

void leds_pwm_set_yellow_brightness(uint8_t brightness);
uint8_t leds_pwm_get_yellow_brightness(void);
~~~

### RGB

RGB components use the range 0…255.

Zero for all three components turns the RGB LED off.

### HSV

The public HSV interface uses:

- hue: 0…359 degrees;
- saturation: 0…100 percent;
- value: 0…100 percent.

A value of 0 turns the RGB LED off.

HSV conversion is implemented inside `leds_pwm`.

### Yellow brightness

Yellow brightness uses 0…100 percent.

Internally this percentage is converted to the 0…255 duty range expected by `pwm`.

Zero turns the yellow LED off; 100 turns it fully on.

## Stored state

The module stores both RGB and HSV representations of the current RGB color, plus the current yellow brightness.

This means:

- after `leds_pwm_set_rgb()`, the RGB getter returns the supplied RGB values and the HSV getter returns their converted representation;
- after `leds_pwm_set_hsv()`, the HSV getter returns the supplied HSV values and the RGB getter returns the converted representation.

The module does not expose PWM duty values because they are an implementation detail.

## LED-to-PWM mapping

~~~ text
PWM channel 0 -> yellow LED
PWM channel 1 -> red LED
PWM channel 2 -> green LED
PWM channel 3 -> blue LED
~~~

All four board LED outputs are active-low.

## Initialization

`leds_pwm_init()` initializes the underlying PWM module at 1000 Hz.

The underlying `pwm` driver configures the GPIO pins itself, so `leds_gpio_init()` is not called.

After successful initialization:

~~~ text
yellow brightness = 0%
RGB                = (0, 0, 0)
HSV                = (0, 0%, 0%)
~~~

The module is intended for one-time initialization. Setter and getter calls before successful initialization are precondition violations.

## Timing

`leds_pwm` has no timers and no processing function.

It only changes the requested output values.

Blinking, mode indication, and other temporal behavior belong to the caller. For example, a caller can implement blinking by alternating yellow brightness between 100% and 0%, or RGB between a selected color and black.

## Error handling

Invalid initialization state, NULL getter pointers, and out-of-range HSV/percentage values are checked with `ASSERT`.

The return value of `leds_pwm_init()` reports failure of the underlying PWM initialization.

## Relationship with leds_gpio

`leds_gpio` remains a separate, standalone GPIO LED module.

It is not used by `leds_pwm`. The two modules provide different ways to control the same board LEDs:

~~~ text
GPIO control:
    application -> leds_gpio -> GPIO

PWM control:
    application -> leds_pwm -> pwm -> PWM peripheral -> GPIO
~~~

This separation allows the PWM LED module to be used without the older GPIO/fade/sequence stack.
