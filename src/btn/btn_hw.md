# btn_hw

`btn_hw` --- нижний аппаратный слой обработки кнопки. Его задача ---
обнаружить изменения физического состояния GPIO, устранить дребезг
контактов и передать в верхний слой только подтверждённое состояние
кнопки.

Модуль не формирует пользовательские события `PRESS`, `RELEASE`,
`SINGLE_PRESS` или `DOUBLE_PRESS`. Он работает только с двумя
состояниями:

-   `pressed` --- кнопка нажата;
-   `released` --- кнопка отпущена.

Подтверждённое состояние передаётся в `btn` через callback
`btn_hw_callback_t`.

## Общая схема

Путь от физического изменения входа до логического callback выглядит
следующим образом:

``` text
Физический GPIO
      │
      │ изменение уровня
      ▼
   GPIOTE
      │
      │ GPIO event / interrupt
      ▼
gpiote_pin_change_callback()
      │
      ├── чтение текущего GPIO
      ├── сохранение debounce candidate
      ├── остановка предыдущего debounce timer
      └── запуск нового debounce timer
                     │
                     │ 10 ms без нового изменения
                     ▼
            RTC1 / app_timer
                     │
                     ▼
       rtc1_debounce_callback()
                     │
                     ├── повторное чтение GPIO
                     │
                     ├── состояние изменилось?
                     │       │
                     │       └── <span style="color:#c62828"><b>да → новый candidate + новый debounce interval</b></span>
                     │
                     └── <span style="color:#2e7d32"><b>состояние стабильно</b></span>
                             │
                             ▼
                     состояние подтверждено
                             │
                             ▼
                    m_callback(state)
                             │
                             ▼
                           btn
```

Здесь красным выделена ветка, по которой debounce продолжается из-за
изменения входа. Зелёная ветка означает, что состояние осталось
стабильным и может быть подтверждено.

------------------------------------------------------------------------

## Используемые аппаратные ресурсы

Для работы `btn_hw` используются:

-   GPIO вход кнопки;
-   GPIOTE для обнаружения изменения состояния GPIO;
-   LFCLK;
-   RTC1 через `app_timer`;
-   interrupt context GPIOTE и RTC1.

Для платы PCA10059 используется:

``` c
BUTTON_1
BUTTON_PULL
BUTTONS_ACTIVE_STATE
```

Кнопка является active-low: при нажатии GPIO имеет значение
`BUTTONS_ACTIVE_STATE`, равное `0`.

Поэтому физический уровень GPIO преобразуется в логическое состояние
кнопки следующим образом:

``` text
GPIO = BUTTONS_ACTIVE_STATE  → <span style="color:#2e7d32"><b>pressed</b></span>
GPIO != BUTTONS_ACTIVE_STATE → <span style="color:#2e7d32"><b>released</b></span>
```

В коде это преобразование выполняется выражением:

``` c
(nrf_gpio_pin_read(m_btn_pin) == BUTTONS_ACTIVE_STATE)
```

------------------------------------------------------------------------

# Инициализация

Инициализация `btn_hw` выполняется в два этапа:

``` c
btn_hw_init(callback);
btn_hw_start();
```

Это разделение существенно.

`btn_hw_init()` выполняет настройку аппаратных ресурсов и создаёт timer,
но ещё не начинает обработку состояния кнопки.

`btn_hw_start()` включает события GPIOTE и запускает первоначальный
debounce.

## `btn_hw_init()`

На момент вызова `btn_hw_init()` уже должны быть готовы:

-   LFCLK;
-   `app_timer`.

Их инициализация находится выше, в `btn_init()`.

Последовательность инициализации выглядит так:

``` text
btn_init()
   │
   ├── clock_init()
   │      ├── nrf_drv_clock_init_check()
   │      ├── nrf_drv_clock_init() if needed
   │      └── <span style="color:#2e7d32"><b>LFCLK запущен</b></span>
   │
   ├── app_timer_init()
   │
   └── btn_hw_init()
          │
          ├── создание debounce timer
          ├── инициализация GPIOTE
          └── настройка GPIO event
```

`btn_hw` поэтому не является самостоятельным модулем и не инициализирует
LFCLK или `app_timer` самостоятельно.

------------------------------------------------------------------------

## Debounce timer

Для debounce создаётся одношотный timer:

``` c
APP_TIMER_DEF(m_debounce_timer);
```

Он создаётся с режимом:

``` c
APP_TIMER_MODE_SINGLE_SHOT
```

и callback:

``` c
rtc1_debounce_callback
```

Период debounce:

``` c
#define BTN_DEBOUNCE_TIME_MS 10
```

Timer не измеряет длительность нажатия кнопки и не используется как
периодический timer.

Его задача значительно проще:

> после обнаруженного изменения GPIO дождаться 10 ms и проверить,
> сохранилось ли состояние.

Поэтому при каждом новом обнаруженном фронте уже запущенный debounce
interval заменяется новым.

------------------------------------------------------------------------

# GPIOTE

Для обнаружения изменения входа используется GPIOTE.

Конфигурация создаётся на основе:

``` c
NRFX_GPIOTE_CONFIG_IN_SENSE_TOGGLE(false)
```

Это означает, что GPIOTE должен реагировать на изменение GPIO в обе
стороны:

``` text
released → pressed
pressed  → released
```

После создания конфигурации вручную задаётся pull-конфигурация:

``` c
config.pull = BUTTON_PULL;
```

Затем вход и callback регистрируются:

``` c
nrfx_gpiote_in_init(
    m_btn_pin,
    &config,
    gpiote_pin_change_callback)
```

`nrfx_gpiote_in_init()` сам выполняет необходимую настройку GPIO как
входа, поэтому отдельный вызов `nrf_gpio_cfg_input()` для кнопки не
требуется.

------------------------------------------------------------------------

# Включение GPIOTE

Сам `nrfx_gpiote_in_init()` ещё не начинает обработку событий кнопки.

Обработка включается в `btn_hw_start()`:

``` c
nrfx_gpiote_in_event_enable(m_btn_pin, true);
```

После этого изменение GPIO может привести к вызову:

``` c
gpiote_pin_change_callback()
```

То есть последовательность имеет две разные стадии:

``` text
nrfx_gpiote_in_init()
        │
        └── настройка входа и обработчика

nrfx_gpiote_in_event_enable()
        │
        └── <span style="color:#2e7d32"><b>фактическое включение обработки событий</b></span>
```

------------------------------------------------------------------------

# Начальный запуск и первоначальный debounce

Особое значение имеет порядок действий в `btn_hw_start()`.

Сначала включается обработка GPIO events:

``` c
nrfx_gpiote_in_event_enable(m_btn_pin, true);
```

После этого считывается текущее состояние GPIO:

``` c
m_debounce_candidate_state =
    (nrf_gpio_pin_read(m_btn_pin) == BUTTONS_ACTIVE_STATE);
```

Это состояние становится первым debounce candidate.

Затем устанавливается:

``` c
m_initial_state_pending = true;
```

и запускается debounce timer на 10 ms.

Таким образом, после старта состояние кнопки **не считается сразу
достоверным**.

Вместо этого оно проходит тот же debounce-механизм, что и последующие
изменения.

Полная последовательность:

``` text
btn_hw_start()
     │
     ├── enable GPIOTE
     │
     ├── read GPIO
     │
     ├── candidate = current GPIO state
     │
     ├── initial_state_pending = true
     │
     └── start debounce timer (10 ms)
                     │
                     ▼
                RTC1 callback
```

Это позволяет корректно обработать ситуацию, когда кнопка уже была
нажата к моменту запуска устройства.

------------------------------------------------------------------------

# Почему GPIOTE включается до чтения начального состояния

GPIOTE включается до первого чтения GPIO намеренно.

После включения GPIOTE любое изменение входа во время первоначального
debounce может быть обнаружено и обработано.

Например:

``` text
t = 0 ms
GPIO = released
GPIOTE enabled
candidate = released
timer = 10 ms

t = 4 ms
пользователь нажал кнопку
GPIO: released → pressed
GPIOTE event
        │
        ▼
gpiote_pin_change_callback()
        │
        ├── candidate = pressed
        ├── stop timer
        └── start timer = 10 ms

t = 14 ms
RTC1 callback
GPIO = pressed
candidate = pressed
        │
        ▼
<span style="color:#2e7d32"><b>pressed подтверждено</b></span>
```

В результате первоначальный `released` не был ошибочно принят за
окончательное состояние.

------------------------------------------------------------------------

# Обработка GPIO event

После физического изменения GPIO GPIOTE вызывает:

``` c
gpiote_pin_change_callback()
```

Callback выполняется в interrupt context.

Первое действие --- повторно прочитать физический GPIO:

``` c
m_debounce_candidate_state =
    (nrf_gpio_pin_read(pin) == BUTTONS_ACTIVE_STATE);
```

Значение сохраняется как текущий debounce candidate.

Затем предыдущий debounce timer останавливается:

``` c
app_timer_stop(m_debounce_timer);
```

и запускается заново:

``` c
app_timer_start(
    m_debounce_timer,
    APP_TIMER_TICKS(BTN_DEBOUNCE_TIME_MS),
    NULL);
```

Таким образом, каждый новый GPIO event начинает новый полный
10-миллисекундный интервал.

------------------------------------------------------------------------

# Почему timer перезапускается

Механизм debounce основан не на том, чтобы просто подождать 10 ms после
первого фронта.

Требование другое:

> GPIO должен оставаться стабильным в течение полного debounce interval.

При дребезге контактов возможна последовательность:

``` text
released
   │
   ├── pressed
   ├── released
   ├── pressed
   ├── released
   └── pressed
```

Каждое такое изменение вызывает GPIOTE callback и перезапускает timer.

Поэтому окончательное подтверждение происходит только после того, как
очередной 10-миллисекундный интервал закончился без нового изменения.

Условно это можно представить так:

``` text
edge      edge       edge
 │         │          │
 ▼         ▼          ▼
[----------][----------][----------]
                         10 ms
                         │
                         ▼
                    <span style="color:#2e7d32"><b>stable</b></span>
```

------------------------------------------------------------------------

# app_timer и RTC1

В данной реализации debounce timer создаётся через `app_timer`.

На уровне аппаратного исполнения `app_timer` использует RTC1.

Поэтому цепочка таймера выглядит следующим образом:

``` text
app_timer_start()
      │
      ▼
app_timer
      │
      ▼
RTC1 compare event
      │
      ▼
RTC1 interrupt
      │
      ▼
app_timer callback
      │
      ▼
rtc1_debounce_callback()
```

Для работы RTC1 timer требуется работающий LFCLK.

`btn_init()` сначала инициализирует общий legacy clock driver, если
он ещё не готов, запускает LFCLK, затем инициализирует `app_timer` и
только после этого передаёт управление `btn_hw`.

``` text
clock_init()
    │
    ├── nrf_drv_clock_init_check()
    ├── nrf_drv_clock_init() if needed
    └── <span style="color:#2e7d32"><b>LFCLK running</b></span>
             │
             ▼
       app_timer_init()
             │
             ▼
       btn_hw_init()
             │
             ▼
       btn_hw_start()
```

Это является частью обязательного порядка инициализации.

------------------------------------------------------------------------

# RTC1 callback

После истечения debounce interval вызывается:

``` c
rtc1_debounce_callback()
```

Этот callback также выполняется в interrupt context.

Он не доверяет значению, которое было сохранено в момент GPIO event.

Вместо этого GPIO читается непосредственно перед подтверждением:

``` c
pin_state =
    (nrf_gpio_pin_read(m_btn_pin) == BUTTONS_ACTIVE_STATE);
```

Затем текущее физическое состояние сравнивается с debounce candidate.

------------------------------------------------------------------------

# Проверка стабильности

Если:

``` c
pin_state != m_debounce_candidate_state
```

значит GPIO изменился во время debounce interval.

В этом случае состояние ещё нельзя считать стабильным.

Новый уровень становится новым candidate:

``` c
m_debounce_candidate_state = pin_state;
```

и запускается ещё один полный debounce interval.

``` text
debounce timer expired
        │
        ▼
    read GPIO
        │
        ▼
GPIO != candidate?
        │
       <span style="color:#c62828"><b>yes</b></span>
        │
        ├── candidate = GPIO
        └── start 10 ms
```

Никакой callback в `btn` в этот момент не вызывается.

------------------------------------------------------------------------

# Подтверждение состояния

Если после истечения debounce interval:

``` c
pin_state == m_debounce_candidate_state
```

считается, что GPIO оставался стабильным весь необходимый интервал.

Состояние может быть подтверждено.

Далее есть два случая:

1.  это первоначальное состояние;
2.  это обычное изменение уже инициализированного состояния.

------------------------------------------------------------------------

# Первоначальное подтверждение

При первоначальном запуске:

``` c
m_initial_state_pending == true
```

После успешного debounce:

``` c
m_confirmed_state = pin_state;
m_initial_state_pending = false;
```

После этого состояние передаётся в callback:

``` c
m_callback(pin_state);
```

На уровне `btn_hw` это первое подтверждённое состояние кнопки.

При этом `btn` специально не генерирует из него `PRESS` или `RELEASE`.

Это позволяет корректно обработать кнопку, которая уже была нажата при
запуске устройства.

Например:

``` text
startup
   │
   ▼
GPIO = pressed
   │
   ▼
10 ms debounce
   │
   ▼
<span style="color:#2e7d32"><b>pressed confirmed</b></span>
   │
   ▼
btn receives initial state
   │
   ▼
logical state initialized as pressed
```

Если пользователь затем отпустит кнопку, это уже будет обычное
подтверждённое изменение:

``` text
pressed
   │
   │ release
   ▼
GPIOTE
   │
   ▼
debounce
   │
   ▼
<span style="color:#2e7d32"><b>released confirmed</b></span>
   │
   ▼
btn
```

------------------------------------------------------------------------

# Обычное изменение состояния

После первоначальной инициализации:

``` c
m_initial_state_pending == false
```

подтверждённое состояние сравнивается с предыдущим:

``` c
if (m_confirmed_state == pin_state) return;
```

Если состояние не изменилось, callback не вызывается.

Если состояние изменилось:

``` c
m_confirmed_state = pin_state;
```

и вызывается:

``` c
m_callback(pin_state);
```

Таким образом, `btn_hw` гарантирует, что верхний слой получает только
реальные подтверждённые изменения состояния.

------------------------------------------------------------------------

# Состояния внутри `btn_hw`

Модуль фактически поддерживает три различных понятия состояния.

### `m_debounce_candidate_state`

Последнее состояние, обнаруженное во время процесса debounce.

Оно ещё не обязательно является подтверждённым.

### `m_confirmed_state`

Последнее состояние GPIO, которое прошло debounce.

Именно это состояние считается достоверным.

### `m_initial_state_pending`

Флаг, показывающий, что подтверждённое состояние ещё не было установлено
после запуска.

Упрощённо:

``` text
GPIO
 │
 │ edge
 ▼
candidate
 │
 │ 10 ms stable
 ▼
<span style="color:#2e7d32"><b>confirmed</b></span>
 │
 ▼
callback
```

------------------------------------------------------------------------

# Interrupt context

Оба основных callback выполняются в interrupt context:

``` text
GPIOTE interrupt
      │
      └── gpiote_pin_change_callback()

RTC1 interrupt
      │
      └── rtc1_debounce_callback()
```

Поэтому обработчики должны оставаться короткими.

В частности, `btn_hw` не должен выполнять здесь длительные операции или
блокирующую работу.

Его задача в interrupt context ограничивается:

-   чтением GPIO;
-   обновлением состояния debounce;
-   управлением timer;
-   передачей подтверждённого состояния в callback верхнего слоя.

Callback `m_callback()` также вызывается непосредственно из
`rtc1_debounce_callback()`, поэтому он наследует тот же interrupt
context.

Это важно для верхнего слоя: callback не следует использовать для вызова
функций, которые предполагают обычный application context или могут
долго выполняться.

------------------------------------------------------------------------

# Приоритеты прерываний

GPIOTE и RTC1 в текущей конфигурации работают с одинаковым приоритетом
IRQ.

Следовательно, callback GPIOTE и callback RTC1 не вытесняют друг друга
по принципу более высокого приоритета.

Это особенно важно понимать при анализе последовательности:

``` text
GPIOTE IRQ
   │
   └── gpiote_pin_change_callback()
```

и:

``` text
RTC1 IRQ
   │
   └── rtc1_debounce_callback()
```

Оба обработчика являются частью interrupt-driven механизма кнопки, но
между ними нет отдельного программного потока.

------------------------------------------------------------------------

# Защита критических секций

`btn_hw_start()` выполняет изменение нескольких связанных состояний:

``` text
GPIOTE event enable
candidate state
initial state flag
debounce timer start
```

Эта последовательность выполняется внутри:

``` c
CRITICAL_REGION_ENTER();
...
CRITICAL_REGION_EXIT();
```

Цель --- не допустить вмешательства другого interrupt handler между
операциями, составляющими одну логическую процедуру запуска.

Аналогично, в `btn` критические секции используются для работы с
очередью событий.

------------------------------------------------------------------------

# Полный сценарий обычного нажатия

Предположим, кнопка была стабильно отпущена.

``` text
1. GPIO = released
       │
       ▼
2. Пользователь нажимает кнопку
       │
       ▼
3. GPIO меняется
       │
       ▼
4. GPIOTE обнаруживает изменение
       │
       ▼
5. gpiote_pin_change_callback()
       │
       ├── GPIO = pressed
       ├── candidate = pressed
       ├── stop debounce timer
       └── start 10 ms
                   │
                   ▼
6. RTC1 timer expires
       │
       ▼
7. rtc1_debounce_callback()
       │
       ├── GPIO = pressed
       ├── GPIO == candidate
       └── <span style="color:#2e7d32"><b>состояние подтверждено</b></span>
                   │
                   ▼
8. confirmed_state = pressed
       │
       ▼
9. m_callback(true)
       │
       ▼
10. btn получает подтверждённое нажатие
```

------------------------------------------------------------------------

# Полный сценарий с дребезгом

При дребезге между первым и окончательным состоянием могут происходить
дополнительные изменения.

``` text
GPIO:

released ──┐
            ├── pressed
            ├── released
            ├── pressed
            ├── released
            └── pressed ───────────── <span style="color:#2e7d32"><b>stable</b></span>
```

Каждый фронт:

``` text
GPIO edge
   │
   ▼
GPIOTE
   │
   ▼
gpiote_pin_change_callback()
   │
   ├── candidate = current GPIO
   ├── stop timer
   └── start 10 ms
```

Последний фронт запускает последний debounce interval.

Только если после него GPIO остаётся стабильным:

``` text
last edge
   │
   ▼
10 ms without edge
   │
   ▼
RTC1 callback
   │
   ▼
<span style="color:#2e7d32"><b>state confirmed</b></span>
```

В результате дребезг не передаётся в `btn` как последовательность ложных
нажатий и отпусканий.

------------------------------------------------------------------------

# Граница ответственности `btn_hw`

`btn_hw` заканчивает свою работу на моменте подтверждения физического
состояния.

Он отвечает за:

``` text
GPIO
 ↓
GPIOTE
 ↓
debounce
 ↓
confirmed physical state
 ↓
callback
```

Он не отвечает за:

-   длительность нажатия;
-   одиночное нажатие;
-   двойное нажатие;
-   последовательности пользовательских событий;
-   очередь событий;
-   интервал double press.

Эти задачи выполняются модулем `btn`.

Таким образом, callback из `btn_hw` в `btn` является границей между
двумя уровнями:

``` text
┌──────────────────────────────┐
│            btn               │
│                              │
│ PRESS / RELEASE              │
│ SINGLE_PRESS / DOUBLE_PRESS  │
│ event queue                  │
└──────────────▲───────────────┘
               │
       confirmed bool
               │
┌──────────────┴───────────────┐
│          btn_hw              │
│                              │
│ GPIO / GPIOTE                │
│ debounce                     │
│ app_timer / RTC1             │
│ initial state                │
└──────────────────────────────┘
```

Именно поэтому `btn_hw` можно рассматривать как аппаратный фильтр и
детектор подтверждённого изменения состояния, а `btn` --- как слой
логической интерпретации этого состояния.