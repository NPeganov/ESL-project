# ESL-project

Проект для nRF52840 на плате PCA10059.

Сборка и прошивка:

```sh
make clean && make && make dfu
```

SDK по умолчанию ожидается в `../esl-nsdk`.

## Структура приложения

- `src/main.c` — точка входа, запуск приложения и обработка отложенных логов.
- `src/hsv_picker/` — прикладная логика HSV color picker.
- `src/hsv_picker/leds_control/` — управление RGB и жёлтым LED для HSV picker.
- `src/leds/` — высокоуровневый аппаратный PWM-драйвер LED и преобразования RGB/HSV.
- `src/hardware/` — низкоуровневые драйверы PWM и GPIO.
- `src/timer_service/` — общая инициализация LFCLK и app_timer.
- `src/btn/` — обработка кнопки.
- `src/log/` — логирование через USB CDC ACM.

## Документация

### Приложение HSV Picker

- [HSV Picker](src/hsv_picker/README.md)
- [leds_control](src/hsv_picker/leds_control/README.md)
- [Публичный координатор leds_control](src/hsv_picker/leds_control/leds_control.md)
- [Контроллер изменения HSV](src/hsv_picker/leds_control/leds_control_hsv.md)
- [Контроллер жёлтого LED](src/hsv_picker/leds_control/leds_control_yellow.md)

### Аппаратные и LED-модули

- [Аппаратный PWM](src/hardware/pwm/pwm.md)
- [GPIO LED driver](src/hardware/leds_gpio/leds_gpio.md)
- [LED modules](src/leds/README.md)
- [leds_pwm](src/leds/leds_pwm.md)
- [color conversion](src/leds/internal/color.md)

### Остальные модули

- [Button](src/btn/README.md)
- [Logging](src/log/README.md)

Исходники в этой ветке синхронизированы по структуре и содержимому с `workshop-7`; Markdown-файлы описывают архитектуру и контракты модулей.
