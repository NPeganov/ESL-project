# ESL-project
Repository for the main ESL-project

To build and flash type `make clean && make && make dfu`.


## Documentation

Основной архитектурный документ проекта:

- [src/main.md](src/main.md) — схема взаимодействия и подробное описание логики всех основных модулей и main loop.

Документация отдельных модулей находится рядом с их исходным кодом:

- [src/btn/btn.md](src/btn/btn.md) — логический уровень кнопки;
- [src/btn/btn_hw.md](src/btn/btn_hw.md) — GPIO, GPIOTE и debounce;
- [src/leds_gpio/led_sequence.md](src/leds_gpio/led_sequence.md) — последовательность LED;
- [src/leds_gpio/led_controller.md](src/leds_gpio/led_controller.md) — управление фазами одного LED;
- [src/leds_gpio/fade.md](src/leds_gpio/fade.md) — плавное изменение яркости;
- [src/leds_gpio/soft_pwm.md](src/leds_gpio/soft_pwm.md) — программный PWM;
- [src/pwm/pwm.md](src/pwm/pwm.md) — универсальная аппаратная PWM-система;
- [src/log/log.md](src/log/log.md) — прикладной интерфейс логирования и USB backend.
