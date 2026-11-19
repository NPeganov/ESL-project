# leds_control

`leds_control` — прикладной модуль управления RGB- и жёлтым светодиодами для HSV Picker. Он предоставляет один публичный API и скрывает внутренние контроллеры изменения HSV и жёлтой индикации.

## Состав

- [leds_control.md](leds_control.md) — публичный фасад, инициализация и общий таймер.
- [leds_control_hsv.md](leds_control_hsv.md) — изменение Hue, Saturation и Value.
- [leds_control_yellow.md](leds_control_yellow.md) — состояния и паттерны жёлтого LED.

## Взаимодействие

```text
                 hsv_picker
                      |
                      v
                 leds_control
                 /          \
                v            v
        leds_control_hsv  leds_control_yellow
                |            |
                +-----+------+
                      |
                      v
                   leds_pwm
                      |
                      v
                 hardware/pwm
```

`leds_control` владеет единственным периодическим таймером. Каждый его callback получает один timestamp и передаёт его обоим внутренним контроллерам. Внутренние контроллеры не создают собственные таймеры.

Оба внутренних модуля являются деталями реализации `leds_control`. Их короткие функции `hsv_*` и `yellow_*` не являются API приложения.
