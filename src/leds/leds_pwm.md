# PWM LED module

## Purpose

`leds_pwm` is a self-contained high-level LED module built on the universal hardware PWM module.

It controls the four LEDs on the PCA10059 board:

~~~ text
PWM channel 0 -> yellow LED
PWM channel 1 -> red LED
PWM channel 2 -> green LED
PWM channel 3 -> blue LED
~~~

The module hides GPIO pins, PWM channels, polarity, PWM frequency, and RGB/HSV conversion from its users.

It does not implement blinking, timers, animations, or application input modes. A caller that needs blinking changes the LED value through this API at the required times.

## Public interface

### Initialization

~~~ c
bool leds_pwm_init(void);
~~~

Initializes the hardware PWM used by all four LEDs.

The module configures:

- PWM frequency: 1000 Hz;
- all four PCA10059 LED GPIOs;
- active-low polarity for all channels.

The underlying `pwm` module configures the GPIO pins itself, so `leds_gpio_init()` is not required.

On successful initialization all LEDs are off.

The module is intended to be initialized once. Calling `leds_pwm_init()` again is a precondition violation.

### RGB color

~~~ c
void leds_pwm_set_rgb(uint8_t red, uint8_t green, uint8_t blue);
~~~

Sets the RGB LED using independent 8-bit components:

- red: 0…255;
- green: 0…255;
- blue: 0…255.

The supplied RGB values are stored as the current RGB representation and converted to HSV for the corresponding getter.

The values are converted to PWM duty values and sent to the three RGB PWM channels.

The color is off when all three components are zero.

### HSV color

~~~ c
void leds_pwm_set_hsv(uint16_t hue, uint8_t saturation, uint8_t value);
~~~

Sets the RGB LED using:

- hue: 0…359 degrees;
- saturation: 0…100 percent;
- value: 0…100 percent.

The supplied HSV values are stored as the current HSV representation and converted to RGB for the corresponding getter.

The resulting RGB components are converted to PWM duty values and sent to the RGB channels.

Hue is cyclic in the conceptual HSV color model, but this API deliberately accepts only 0…359. Invalid values are checked with `ASSERT`.

A value of 0 turns the RGB LED off regardless of hue and saturation.

### Reading RGB

~~~ c
void leds_pwm_get_rgb(uint8_t *red, uint8_t *green, uint8_t *blue);
~~~

Returns the current RGB representation through the supplied pointers.

The pointers must be non-NULL.

### Reading HSV

~~~ c
void leds_pwm_get_hsv(uint16_t *hue, uint8_t *saturation, uint8_t *value);
~~~

Returns the current HSV representation through the supplied pointers.

The pointers must be non-NULL.

The module stores both RGB and HSV representations. Therefore a value returned by a getter is the representation associated with the most recent setter, rather than a newly reconstructed value from the other color space.

### Yellow brightness

~~~ c
void leds_pwm_set_yellow_brightness(uint8_t brightness);
~~~

Sets yellow LED brightness from 0…100 percent.

The percentage is converted internally to the PWM module's 0…255 duty range.

- 0% = off;
- 100% = fully on.

~~~ c
uint8_t leds_pwm_get_yellow_brightness(void);
~~~

Returns the current yellow LED brightness in percent.

## Color conversion

RGB uses the hardware PWM range directly:

~~~ text
RGB component: 0...255
        |
        v
PWM duty:      0...255
~~~

HSV saturation and value use percentages at the public interface:

~~~ text
Saturation: 0...100 %
Value:      0...100 %
        |
        v
internal RGB: 0...255
        |
        v
PWM duty:      0...255
~~~

The same percentage conversion is used for yellow brightness.

HSV conversion is implemented inside `leds_pwm`. It is not a separate public module because color conversion is part of the responsibility of this LED abstraction.

## State

The module keeps the current values of:

- RGB color;
- HSV color;
- yellow brightness.

The RGB and HSV states are updated together by the corresponding setter. This makes both getters independent of later changes to the other representation.

No current-value getter is provided for PWM duty itself because duty is an implementation detail of `leds_pwm`.

## Timing and blinking

`leds_pwm` has no timing state.

For example, application code can implement blinking by alternating:

~~~ c
leds_pwm_set_yellow_brightness(100);
leds_pwm_set_yellow_brightness(0);
~~~

or by changing the RGB color between a color and black.

The module only performs the requested output changes. It does not decide when they should occur.

## Dependencies

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
nRF52840 PWM peripheral
~~~

`leds_pwm` knows the board-specific LED assignment because it is the board-specific LED abstraction. The universal `pwm` module remains unaware of LEDs.

`leds_gpio` is a separate GPIO-only LED module and is not used by `leds_pwm`.

## Initial state

After successful `leds_pwm_init()`:

~~~ text
yellow brightness = 0%
RGB                = (0, 0, 0)
HSV                = (0, 0%, 0%)
~~~

All physical LEDs are off.
