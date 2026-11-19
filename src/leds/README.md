# LED modules

Каталог `leds` содержит высокоуровневое управление физическими светодиодами через аппаратный PWM и вычислительные зависимости этого управления.

## Модули

- [leds_pwm.md](leds_pwm.md) — управление жёлтым и RGB-светодиодами через аппаратный PWM.
- [internal/color.md](internal/color.md) — целочисленные преобразования RGB/HSV и процентов.

## Архитектура

```text
Application
    |
    v
leds_pwm
    |
    +------> internal/color
    |
    v
hardware/pwm
    |
    v
nRF52840 PWM0
    |
    v
LED GPIO pins
```

`leds_pwm` — привязанный к PCA10059 уровень управления LED. Он знает назначение четырёх PWM-каналов, активную полярность и формат RGB/HSV, но не реализует прикладные режимы или временную индикацию.

`hardware/pwm` остаётся универсальным драйвером и не знает, что подключено к его каналам.

## Прикладная логика HSV Picker

Управление изменением HSV и паттернами жёлтого индикатора не входит в `leds_pwm`. Оно находится в приложении HSV Picker:

- [leds_control](../hsv_picker/leds_control/README.md) координирует оба контроллера и общий периодический таймер;
- [leds_control_hsv](../hsv_picker/leds_control/leds_control_hsv.md) изменяет выбранный HSV-компонент;
- [leds_control_yellow](../hsv_picker/leds_control/leds_control_yellow.md) управляет состоянием и миганием жёлтого LED.

Это разделение позволяет использовать `leds_pwm` независимо от логики конкретного приложения.
