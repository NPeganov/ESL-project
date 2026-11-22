# hardware/pwm — аппаратный PWM-драйвер

## Назначение

`hardware/pwm` — низкоуровневая обёртка над nRF52840 PWM0 через nrfx. Она предоставляет четыре канала с единым API duty 0…255 и скрывает настройку периферии, active-low инверсию и формат sequence.

Драйвер не знает назначения каналов и не управляет логикой LED. Привязка каналов к жёлтому и RGB LED находится в `leds_pwm`.

## Публичный API

```c
bool pwm_init(pwm_config_t const *config);
void pwm_set(pwm_channel_t channel, uint8_t duty);
```

`pwm_init()` получает конфигурацию четырёх GPIO pins и полярности. Все каналы начинают с нулевого duty. Возвращает `false`, если nrfx PWM driver не удалось инициализировать.

`pwm_set()` задаёт duty выбранного канала: 0 — неактивный уровень, 255 — активный уровень. Смысл значения одинаков для active-high и active-low конфигурации.

## Фиксированная временная конфигурация

Частота не задаётся вызывающим кодом. Внутренние параметры:

| Параметр | Значение |
|---|---:|
| Base clock | 1 МГц |
| PWM frequency | 1 кГц |
| TOP | 999 |
| Duty scale | 0…255 |
| Интервал обновления sequence | 10 мс |
| IRQ priority | 6 |

TOP вычисляется как `base_clock_hz / frequency_hz - 1`. Драйвер работает в up-counting mode, individual load mode и automatic step mode.

## Полярность и преобразование duty

Для каждого канала драйвер запоминает active-low флаг и передаёт nrfx pin inversion flag, если выход активный низким уровнем.

Логический duty 0…255 масштабируется в диапазон 0…TOP с округлением к ближайшему timer tick. Compare value вычисляется относительно TOP; дополнительный polarity bit задаёт необходимую полярность compare value для active-low канала.

Таким образом, вызывающий модуль не должен инвертировать яркость или учитывать электрическую полярность GPIO.

## Sequence playback

Драйвер хранит один `nrf_pwm_values_individual_t` буфер и одну `nrf_pwm_sequence_t`. В individual load mode структура задаёт значения всех четырёх каналов.

Для непрерывного воспроизведения вызывается `nrfx_pwm_simple_playback()` с одной логической sequence и флагом LOOP. Simple playback использует оба аппаратных sequence slot для циклического воспроизведения; это не означает, что приложение поддерживает две независимые sequence или два буфера.

Поэтому включены оба события `NRFX_PWM_EVT_END_SEQ0` и `NRFX_PWM_EVT_END_SEQ1`. Обработчик учитывает оба события и периодически обновляет общий sequence buffer. Новые значения duty записываются через `pwm_set()` в отдельный массив, а sequence buffer синхронизируется с ними в callback примерно каждые 10 мс.

## Поток данных

```text
leds_pwm
   |
   +--> pwm_set(channel, duty)
             |
             v
       m_duty[channel]
             |
             v
    playback event handler
             |
             v
    sequence compare values
             |
             v
       nRF PWM0 outputs
```

PWM продолжает генерировать сигнал аппаратно; обновление duty не требует polling из main loop.

## Ограничения

- Используется PWM instance 0.
- Частота и период обновления фиксированы.
- Публичного API для остановки, перенастройки частоты или изменения sequence нет.
- GPIO pin и polarity должны быть корректными; конфигурация проверяется ASSERT.
- `pwm_init()` рассчитан на однократный вызов.
- Драйвер не инициализирует отдельный GPIO-драйвер: nrfx PWM настраивает собственные output pins.
