# HSV Picker application

Каталог содержит прикладную реализацию HSV color picker.

## Файлы

- `hsv_picker.h` — внешний интерфейс приложения.
- `hsv_picker.c` — режимы выбора HSV, обработка событий кнопки, восстановление/сохранение цвета и начальная настройка.
- [hsv_picker.md](hsv_picker.md) — подробное описание архитектуры, инициализации и поведения приложения.
- [leds_control](leds_control/README.md) — прикладной уровень управления RGB и жёлтым LED.
- [nvm_log](../nvm_log/nvm_log.md) — хранение последнего HSV payload в Application Data Area.

## Инициализация и запуск

Внешний код вызывает две функции:

```c
bool hsv_picker_init(void);
void hsv_picker_start(void);
```

`hsv_picker_init()` инициализирует NVM log, timer service, `leds_control`, восстанавливает сохранённый цвет (или формирует default из `DEVICE_ID`) и регистрирует обработчик кнопки.

`hsv_picker_start()` выключает жёлтый LED и запускает обработку кнопки. `hsv_picker_process()` выполняет отложенное сохранение HSV из основного цикла.

## Режимы управления

Двойное нажатие переключает режимы по циклу:

1. HUE — жёлтый LED мигает медленно;
2. SATURATION — жёлтый LED мигает быстро;
3. BRIGHTNESS — жёлтый LED горит постоянно;
4. NO_INPUT — жёлтый LED выключен.

При удержании кнопки изменяется выбранный HSV-компонент. Реализация изменения параметров и жёлтой индикации вынесена в [leds_control](leds_control/README.md).
