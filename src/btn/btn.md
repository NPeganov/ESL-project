# btn

`btn` — верхний логический слой обработки кнопки. Он получает от
`btn_hw` только подтверждённые изменения физического состояния и
преобразует их в события, которые может получать остальной код
приложения.

`btn` не работает непосредственно с GPIO и не занимается debounce.
Его входом является callback:

~~~ c
void button_state_changed(bool pressed);
~~~

Здесь `pressed == true` означает подтверждённое нажатие, а
`pressed == false` — подтверждённое отпускание.

На выходе `btn` формирует события:

- `BTN_EVENT_PRESS`;
- `BTN_EVENT_RELEASE`;
- `BTN_EVENT_SINGLE_PRESS`;
- `BTN_EVENT_DOUBLE_PRESS`.

События помещаются во внутреннюю очередь и извлекаются через
`btn_event_take()`.

## Общая схема

~~~ text
GPIO
 │
 ▼
btn_hw
 │
 │ confirmed bool
 ▼
button_state_changed()
 │
 ├── PRESS
 ├── RELEASE
 ├── SINGLE_PRESS
 └── DOUBLE_PRESS
 │
 ▼
event queue
 │
 ▼
btn_event_take()
 │
 ▼
application
~~~

Граница между уровнями принципиальна:

> `btn_hw` отвечает на вопрос «какое физическое состояние кнопки
> подтверждено?», а `btn` отвечает на вопрос «какое событие означает
> это состояние для приложения?».

---

## Инициализация

Внешний код вызывает только:

~~~ c
btn_init();
~~~

Внутри выполняется следующая последовательность:

~~~ text
btn_init()
   │
   ├── clock_init()
   │      ├── nrf_drv_clock_init_check()
   │      ├── nrf_drv_clock_init() if needed
   │      └── LFCLK request/start
   │
   ├── app_timer_init()
   │
   ├── btn_hw_init(button_state_changed)
   │      └── hardware layer
   │
   ├── create double-press timer
   │
   └── btn_hw_start()
~~~

Таким образом, `btn` владеет общей инициализацией, необходимой обоим
уровням.

`btn_hw` при этом остаётся внутренним нижним слоем `btn`, а не
самостоятельным публичным модулем.

### LFCLK и app_timer

Для работы таймеров `btn` требуется LFCLK.

`clock_init()` инициализирует общий legacy clock driver, если он ещё
не инициализирован, и запускает LFCLK. После этого вызывается
`app_timer_init()`, и только затем создаются таймеры `btn_hw` и `btn`.

~~~ text
LFCLK
  │
  ▼
app_timer
  │
  ├── btn_hw debounce timer
  │
  └── btn double-press timer
~~~

---

# Первоначальное состояние кнопки

Особенность обработки кнопки состоит в том, что первое подтверждённое
состояние после запуска не считается пользовательским событием.

Это состояние используется только для инициализации логического
состояния `btn`.

Например, если при запуске кнопка уже нажата:

~~~ text
startup
   │
   ▼
btn_hw debounce
   │
   ▼
pressed confirmed
   │
   ▼
button_state_changed(true)
   │
   ▼
m_state_initialized = true
m_press_started = true
   │
   ▼
no PRESS event
~~~

Это важно: сам факт того, что кнопка была физически нажата во время
запуска, не считается новым нажатием для приложения.

Если после этого пользователь отпустит кнопку, отпускание уже будет
обычным завершением цикла:

~~~ text
initial state = pressed
        │
        │ release
        ▼
RELEASE
        │
        ▼
SINGLE_PRESS
~~~

Таким образом, начальное состояние проходит полный debounce в
`btn_hw`, но первое подтверждение не создаёт `PRESS` или
`RELEASE`.

---

# Логическое состояние кнопки

Для обработки последовательности нажатий `btn` поддерживает несколько
внутренних состояний.

### `m_state_initialized`

Показывает, было ли уже получено первое подтверждённое состояние.

До его установки первый callback только инициализирует состояние.

### `m_press_started`

Показывает, находится ли кнопка в текущем цикле нажатия:

- `true` после подтверждённого нажатия;
- `false` после подтверждённого отпускания.

Именно этот флаг позволяет отличить завершённый цикл
`PRESS → RELEASE` от простого подтверждённого отпускания.

### `m_double_press_pending`

Показывает, что после первого завершённого цикла ещё существует окно,
в котором может завершиться второй цикл и тем самым сформировать
`DOUBLE_PRESS`.

### `m_first_release_tick`

Содержит момент первого подтверждённого отпускания, от которого
отсчитывается double-press interval.

Упрощённо состояние можно представить так:

~~~ text
confirmed pressed
       │
       ▼
m_press_started = true
       │
       │ confirmed release
       ▼
m_press_started = false
m_double_press_pending = true
       │
       │ second completed cycle
       ▼
DOUBLE_PRESS
~~~

---

# Подтверждённое нажатие

Когда `btn_hw` сообщает:

~~~ c
pressed == true
~~~

вызывается:

~~~ c
button_state_changed(true);
~~~

Если начальное состояние уже было установлено:

1. `m_press_started` устанавливается в `true`;
2. в очередь помещается `BTN_EVENT_PRESS`;
3. обработка callback завершается.

То есть подтверждённое нажатие всегда даёт:

~~~ text
confirmed pressed
       │
       ▼
PRESS
~~~

При этом `btn` не измеряет длительность самого нажатия.

---

# Подтверждённое отпускание

При:

~~~ c
pressed == false
~~~

сначала в очередь помещается:

~~~ c
BTN_EVENT_RELEASE
~~~

Затем проверяется `m_press_started`.

Если он равен `false`, завершённого цикла нажатия нет, поэтому
дальнейшая логика цикла не выполняется.

Если `m_press_started == true`, отпускание завершает цикл:

~~~ text
PRESS
  │
  │ button remains pressed
  │
  ▼
RELEASE
  │
  ▼
completed press-release cycle
~~~

После этого:

~~~ c
m_press_started = false;
~~~

и в очередь помещается:

~~~ c
BTN_EVENT_SINGLE_PRESS
~~~

Здесь важно значение имени `SINGLE_PRESS`.

> `SINGLE_PRESS` означает любой завершённый цикл
> `PRESS → RELEASE`, а не только короткое нажатие.

Поэтому долгое удержание тоже заканчивается `SINGLE_PRESS`.

---

# Длительное удержание

`btn` не формирует отдельного события для long press.

Например:

~~~ text
PRESS
  │
  │
  │  button held
  │
  │
RELEASE
  │
  ▼
SINGLE_PRESS
~~~

Длительность удержания сама по себе не меняет семантику цикла.

---

# Double press

Double press строится поверх уже завершённых циклов
`PRESS → RELEASE`.

После первого завершённого цикла `btn`:

1. помещает `SINGLE_PRESS` в очередь;
2. запоминает момент первого release;
3. устанавливает `m_double_press_pending = true`;
4. запускает одношотный double-press timer.

~~~ text
first PRESS
    │
    ▼
first RELEASE
    │
    ▼
SINGLE_PRESS
    │
    ├── remember release time
    ├── pending = true
    └── start double-press timer
~~~

Окно задаётся:

~~~ c
BTN_DOUBLE_PRESS_TIME_MS
~~~

По умолчанию оно равно 1000 ms.

---

# Почему double press начинается с RELEASE

Для double press учитываются только завершённые циклы.
Поэтому одного второго `PRESS` недостаточно.

~~~ text
first PRESS
first RELEASE
    │
    ▼
second PRESS
    │
    │ button is still held
    ▼
no DOUBLE_PRESS yet
    │
    ▼
second RELEASE
    │
    ▼
DOUBLE_PRESS
~~~

Это соответствует определению double press как двух завершённых
нажатий, а не просто двух физических фронтов.

Незавершённое удержание не считается вторым нажатием.

---

# Проверка double-press interval

При каждом подтверждённом отпускании вычисляется текущее значение
таймера:

~~~ c
now = app_timer_cnt_get();
~~~

Если уже существует ожидающее окно, сравнивается разница между
текущим моментом и первым release:

~~~ c
app_timer_cnt_diff_compute(now, m_first_release_tick)
~~~

Событие `DOUBLE_PRESS` формируется, если эта разница меньше:

~~~ c
APP_TIMER_TICKS(BTN_DOUBLE_PRESS_TIME_MS)
~~~

Таким образом, проверяется интервал между подтверждёнными
отпусканиями.

---

# Второй завершённый цикл внутри окна

Если второй цикл завершился вовремя:

~~~ text
first RELEASE
     │
     │ < double-press interval
     │
second RELEASE
     │
     ▼
DOUBLE_PRESS
~~~

`btn`:

- сбрасывает `m_double_press_pending`;
- останавливает double-press timer;
- помещает `BTN_EVENT_DOUBLE_PRESS` в очередь.

При этом уже сформированный `SINGLE_PRESS` первого цикла не удаляется.

Порядок событий для двух завершённых циклов:

~~~ text
PRESS
RELEASE
SINGLE_PRESS
PRESS
RELEASE
SINGLE_PRESS
DOUBLE_PRESS
~~~

То есть `DOUBLE_PRESS` является дополнительным событием,
описывающим связь двух циклов, а не заменяет два `SINGLE_PRESS`.

---

# Истечение double-press window

Если второй завершённый цикл не произошёл в течение заданного
интервала, срабатывает:

~~~ c
double_press_timer_handler()
~~~

Он просто сбрасывает:

~~~ c
m_double_press_pending = false;
~~~

После этого следующий завершённый цикл начинает новое окно:

~~~ text
first release
     │
     ├── double-press timer expires
     │
     ▼
pending = false
     │
     │ later completed cycle
     ▼
new first release
     │
     ▼
new double-press window
~~~

Само истечение окна не создаёт никакого события.

---

# Новое окно после второго release вне интервала

Если второй release произошёл уже после истечения первого окна,
он не становится вторым release для double press.

Вместо этого он становится новым первым release:

~~~ text
release #1
    │
    ├── window expires
    │
    ▼
pending = false

release #2
    │
    ▼
new first release
    │
    ▼
new window
~~~

При этом release #2 всё равно завершает свой собственный
`PRESS → RELEASE` цикл и поэтому сопровождается `SINGLE_PRESS`.

---

# Double-press timer

Для double press используется отдельный одношотный timer:

~~~ c
APP_TIMER_DEF(m_double_press_timer);
~~~

Он создаётся в `btn_init()` после инициализации `app_timer`.

Timer нужен не для определения самого double press. Он выполняет только
одну вспомогательную задачу:

> сообщить `btn`, что время ожидания второго завершённого цикла
> закончилось.

Сама проверка второго release выполняется непосредственно в
`button_state_changed()` с использованием значения RTC/app_timer.

Timer является одношотным, потому что новое окно запускается явно после
каждого подходящего release.

---

# Очередь событий

События не передаются приложению напрямую.

Они помещаются во внутреннюю кольцевую очередь:

~~~ c
static btn_event_t m_event_queue[BTN_EVENT_QUEUE_SIZE];
~~~

Очередь имеет:

- `m_event_head` — позицию записи;
- `m_event_tail` — позицию чтения;
- `m_event_count` — количество непрочитанных событий.

Размер очереди задаётся:

~~~ c
BTN_EVENT_QUEUE_SIZE
~~~

и по умолчанию равен 8.

Когда `m_event_head` или `m_event_tail` достигает конца массива,
индекс возвращается к нулю.

Поэтому очередь является кольцевой.

---

# Переполнение очереди

Если очередь заполнена, новые события отбрасываются:

~~~ text
queue full
   │
   ▼
new event
   │
   └── discarded
~~~

Уже накопленные непрочитанные события при этом сохраняются.

Это сделано намеренно: при переполнении предпочтение отдаётся
сохранению старых событий, которые приложение ещё не успело обработать.

Следовательно, `btn` не гарантирует, что при длительной задержке
приложения все события будут сохранены.

---

# `btn_event_take()`

Приложение получает события через:

~~~ c
btn_event_t btn_event_take(void);
~~~

Функция возвращает самое старое непрочитанное событие.

Если очередь пуста:

~~~ c
BTN_EVENT_NONE
~~~

Например:

~~~ text
queue:
PRESS
RELEASE
SINGLE_PRESS

btn_event_take()
    │
    └── PRESS

queue:
RELEASE
SINGLE_PRESS
~~~

Порядок событий внутри очереди сохраняется.

---

# Защита очереди

Очередь используется из разных контекстов:

- события помещаются в неё из callback обработки кнопки;
- события извлекаются приложением через `btn_event_take()`.

Поэтому операции с очередью защищены:

~~~ c
CRITICAL_REGION_ENTER();
...
CRITICAL_REGION_EXIT();
~~~

Цель критической секции — не допустить изменения общей структуры
очереди в середине другой операции.

---

# Interrupt context

`button_state_changed()` вызывается из `btn_hw` после подтверждения
состояния кнопки. Поскольку callback `btn_hw` вызывается из
`rtc1_debounce_callback()`, логика `btn` также выполняется в
interrupt context.

Это означает, что:

- `button_state_changed()` должна оставаться короткой;
- `event_queue_push()` выполняет только необходимую работу с очередью;
- нельзя выполнять здесь длительные операции;
- приложение не должно вызывать тяжёлую логику непосредственно из
  callback.

Double-press timer callback также выполняется в interrupt context и
делает только изменение `m_double_press_pending`.

Фактическое использование событий приложением происходит позднее,
через `btn_event_take()`.

---

# Полный сценарий одного нажатия

Предположим, начальное состояние уже подтверждено как released.

~~~ text
GPIO
 │
 ▼
btn_hw
 │
 └── confirmed pressed
          │
          ▼
button_state_changed(true)
          │
          ├── m_press_started = true
          └── queue: PRESS
                    │
                    ▼
             confirmed released
                    │
                    ▼
button_state_changed(false)
          │
          ├── queue: RELEASE
          ├── m_press_started = false
          ├── queue: SINGLE_PRESS
          ├── remember release tick
          └── start double-press window
~~~

В итоге очередь получает:

~~~ text
PRESS
RELEASE
SINGLE_PRESS
~~~

а затем начинается ожидание возможного второго цикла.

---

# Полный сценарий double press

Два завершённых цикла происходят внутри одного double-press window:

~~~ text
first PRESS
     │
     ▼
first RELEASE
     │
     ├── SINGLE_PRESS
     └── start window
             │
             ▼
second PRESS
     │
     ▼
second RELEASE
     │
     ├── SINGLE_PRESS
     └── DOUBLE_PRESS
~~~

Итоговый порядок:

~~~ text
PRESS
RELEASE
SINGLE_PRESS
PRESS
RELEASE
SINGLE_PRESS
DOUBLE_PRESS
~~~

Таким образом, приложение получает как события каждого отдельного
цикла, так и дополнительный признак того, что два цикла образовали
double press.

---

# Полный сценарий с удержанием

Если кнопку удерживать долго:

~~~ text
PRESS
   │
   │
   │ long hold
   │
   ▼
RELEASE
   │
   ▼
SINGLE_PRESS
~~~

Никакого отдельного long-press события не появляется.

Если после release пользователь быстро выполняет ещё один
`PRESS → RELEASE`, второй цикл может сформировать `DOUBLE_PRESS`
точно так же, как после короткого первого нажатия.

---

# Граница ответственности `btn`

`btn` начинается с подтверждённого значения:

~~~ text
bool pressed
    │
    ▼
button_state_changed()
~~~

и заканчивается на очереди логических событий:

~~~ text
confirmed physical state
          │
          ▼
        btn
          │
          ├── PRESS
          ├── RELEASE
          ├── SINGLE_PRESS
          └── DOUBLE_PRESS
          │
          ▼
     event queue
~~~

`btn` не отвечает за:

- GPIO;
- GPIOTE;
- debounce;
- чтение физического уровня;
- устранение дребезга.

Эти задачи находятся в `btn_hw`.

В свою очередь, `btn_hw` не знает ничего о:

- `SINGLE_PRESS`;
- `DOUBLE_PRESS`;
- double-press interval;
- очереди пользовательских событий.

Таким образом, два уровня имеют чёткую границу:

~~~ text
┌─────────────────────────────────┐
│              btn                │
│                                 │
│ confirmed bool                  │
│       │                         │
│       ▼                         │
│ PRESS / RELEASE                 │
│ SINGLE_PRESS / DOUBLE_PRESS     │
│       │                         │
│       ▼                         │
│ event queue                     │
└───────────────▲─────────────────┘
                │
        callback(bool pressed)
                │
┌───────────────┴─────────────────┐
│            btn_hw               │
│                                 │
│ GPIO                            │
│ GPIOTE                          │
│ debounce                        │
│ initial state                   │
│ confirmed physical state        │
└─────────────────────────────────┘
~~~

Именно эта граница позволяет рассматривать `btn_hw` как аппаратный
слой подтверждения физического состояния, а `btn` — как логический
слой интерпретации этого состояния.

---

# Рекомендуемый порядок чтения

Для понимания всей обработки кнопки документацию лучше читать в таком
порядке:

1. [`btn_hw.md`](btn_hw.md) — как физическое состояние обнаруживается
   и проходит debounce;
2. [`btn.md`](btn.md) — как подтверждённое состояние преобразуется в
   логические события.

После этого [`README.md`](README.md) можно использовать как краткую
точку входа и схему взаимосвязи двух уровней.
