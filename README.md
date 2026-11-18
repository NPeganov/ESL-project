# ESL-project

Repository for the ESL-project.

Build and flash:

```sh
make clean && make && make dfu
```

## Documentation

Документация размещена рядом с соответствующими модулями.

### Hardware PWM and LEDs

- [Аппаратный PWM](src/hardware/pwm/pwm.md) — универсальный драйвер PWM0, выбор таймингов, полярность, EasyDMA и обновление duty.
- [LED modules](src/leds/README.md) — структура актуальной LED-подсистемы.
- [leds_pwm](src/leds/leds_pwm.md) — управление жёлтым и RGB-светодиодами, RGB/HSV API и состояние.
- [color](src/leds/internal/color.md) — внутренние целочисленные преобразования RGB/HSV.
- [leds_gpio](src/leds/leds_gpio.md) — прямое управление GPIO.
- [Legacy software LED modules](src/leds_soft/README.md) — сохранённая документация прежней реализации soft PWM, fade и LED sequence.

### Other modules

- [Button](src/btn/README.md) — документация логического и аппаратного уровней кнопки.
- [Logging](src/log/README.md) — прикладной интерфейс логирования и USB backend.

Точка входа `main.c` в этой ветке является демонстрационной программой и отдельно не документируется.
