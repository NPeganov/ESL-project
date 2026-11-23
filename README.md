# ESL-project

Приложение HSV color picker для nRF52840 на плате PCA10059.

## Сборка и прошивка

SDK ожидается в `../esl-nsdk`.

```sh
make clean && make
make dfu
```

## Структура

- `src/main.c` — точка входа и основной цикл.
- `src/hsv_picker/` — пользовательские режимы HSV Picker.
- `src/nvm_log/` — одностраничный журнал 32-битных записей в Application Data Area.
- `src/hsv_picker/leds_control/` — изменение HSV и индикация жёлтым LED.
- `src/btn/` — аппаратный debounce и логические события кнопки.
- `src/leds/` — RGB/HSV и LED PWM abstraction.
- `src/hardware/pwm/` — низкоуровневый nRF PWM driver.
- `src/hardware/leds_gpio/` — независимый GPIO LED driver.
- `src/timer_service/` — общая инициализация LFCLK/app_timer.
- `src/log/` — deferred logging через USB CDC ACM.

## Документация

Главный документ — [сквозное описание приложения](src/main.md). Он объясняет архитектуру и весь путь от запуска приложения и действий пользователя до физических PWM-выходов.

Документация отдельных модулей:

- [HSV Picker](src/hsv_picker/hsv_picker.md)
- [NVM log](src/nvm_log/nvm_log.md)
- [Button subsystem](src/btn/README.md), [btn](src/btn/btn.md), [btn_hw](src/btn/btn_hw.md)
- [LED control facade](src/hsv_picker/leds_control/leds_control.md)
- [HSV controller](src/hsv_picker/leds_control/leds_control_hsv.md)
- [Yellow LED controller](src/hsv_picker/leds_control/leds_control_yellow.md)
- [LED PWM driver](src/leds/leds_pwm.md), [color conversion](src/leds/internal/color.md)
- [Hardware PWM](src/hardware/pwm/pwm.md)
- [GPIO LED driver](src/hardware/leds_gpio/leds_gpio.md)
- [Timer service](src/timer_service/timer_service.md)
- [Logging](src/log/log.md)

Исходный код в этой ветке не является самостоятельной версией приложения; документация сверяется с актуальной реализацией приложения в ветке `develop`.
