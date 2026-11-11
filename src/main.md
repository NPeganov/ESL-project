# main

main.c является точкой сборки приложения и верхним уровнем управления текущей прошивкой. Модуль не реализует алгоритмы работы кнопки, светодиодов, PWM, fade или последовательности напрямую. Его задача — создать и инициализировать экземпляр приложения, установить правильный порядок инициализации зависимостей, а затем постоянно обслуживать LED-последовательность и обрабатывать события кнопки.

Текущая архитектура является polling-приложением: main() не использует RTOS-потоки и не передаёт управление LED-последовательностью аппаратному обработчику прерываний. Главный цикл регулярно вызывает led_sequence_process(), а кнопочный модуль доставляет в главный цикл уже подтверждённые логические события через очередь.

---

# Место main в архитектуре

Главный принцип архитектуры:

~~~ text
                         APPLICATION
                             |
                             v
                         +--------+
                         |  main  |
                         +--------+
                          /      \
                         /        \
                        v          v
                 button path    LED path
                      |              |
                      v              v
                    btn       led_sequence
                      |              |
                      v              v
                  btn_hw       led_controller
                      |           /       \
                      v          v         v
                  GPIOTE       fade     soft_pwm
                      |          |         |
                      |          +----+----+
                      |               |
                      v               v
                 BUTTON GPIO    PWM output callback
                                      |
                                      v
                                    leds
                                      |
                                      v
                                   LED GPIO
                                      |
                                      v
                                physical LED
~~~

main является точкой соединения двух подсистем:

1. кнопочной подсистемы, превращающей физические изменения GPIO в логические события;
2. LED-подсистемы, превращающей параметры приложения в последовательность физических состояний LED.

На уровне приложения эти подсистемы связаны только через событие BTN_EVENT_DOUBLE_PRESS: двойное нажатие переключает LED-последовательность между запущенным и остановленным состоянием.

---

# Полная схема работы приложения

Ниже приведена расширенная схема от main до самых нижних используемых модулей. Она показывает структуру зависимостей, потоки управления, callback-и и два независимых временных ресурса.

~~~ mermaid
flowchart TD

    MAIN["main()"]

    subgraph INIT["Инициализация"]
        LED_INIT["leds_init()"]
        SYSTICK_INIT["nrfx_systick_init()"]
        BTN_INIT["btn_init()"]
        SEQ_INIT["led_sequence_init()"]

        LED_INIT --> SYSTICK_INIT
        SYSTICK_INIT --> BTN_INIT
        BTN_INIT --> SEQ_INIT
    end

    MAIN --> LED_INIT

    subgraph BUTTON["Кнопочная подсистема"]
        BTN["btn"]
        CLOCK["nrf_drv_clock / LFCLK"]
        TIMER["app_timer"]
        HW["btn_hw"]
        GPIOTE["nrfx_gpiote / GPIOTE"]
        GPIO_BTN["BUTTON GPIO"]

        BTN --> CLOCK
        BTN --> TIMER
        BTN --> HW
        HW --> TIMER
        HW --> GPIOTE
        GPIOTE --> GPIO_BTN

        HW -->|"confirmed state callback"| BTN
        BTN -->|"event queue"| MAIN
    end

    BTN_INIT --> BTN

    subgraph LED_SEQUENCE["Верхний уровень LED"]
        SEQ["led_sequence"]
        DEVICE["DEVICE_ID"]
        SEQ_STATE["LED index / prepared / running"]
        INTER["inter-LED pause"]

        DEVICE --> SEQ
        SEQ --> SEQ_STATE
        SEQ --> INTER
    end

    SEQ_INIT --> SEQ

    subgraph CONTROLLER["Один LED: led_controller"]
        CTRL["led_controller"]
        PHASE["FADE_IN / FADE_OUT / PAUSE"]
        CYCLES["cycle_count / cycles_completed"]
        CTRL --> PHASE
        CTRL --> CYCLES
    end

    SEQ --> CTRL

    subgraph PWM_FADE["Яркость и PWM"]
        FADE["fade"]
        PWM["soft_pwm"]
        FADE -->|"soft_pwm_set_duty()"| PWM
        PWM -->|"output(state, context)"| CALLBACK["pwm_led_output()"]
    end

    CTRL --> FADE
    CTRL --> PWM
    PWM --> CALLBACK

    subgraph LED_HW["Физические LED"]
        LEDS["leds"]
        MAP["LEDS_LIST / LEDS_ACTIVE_STATE"]
        GPIO_LED["LED GPIO"]
        PHYSICAL["Physical LED"]

        CALLBACK --> LEDS
        LEDS --> MAP
        LEDS --> GPIO_LED
        GPIO_LED --> PHYSICAL
    end

    subgraph TIME["Общий временной ресурс"]
        SYSTICK["SysTick"]
    end

    SYSTICK_INIT --> SYSTICK
    SYSTICK --> PWM
    SYSTICK --> FADE
    SYSTICK --> CTRL
    SYSTICK --> INTER

    MAIN -->|"main loop"| SEQ
    MAIN -->|"btn_event_take()"| BTN
    BTN -->|"DOUBLE_PRESS"| MAIN
    MAIN -->|"pause / resume"| SEQ

    SEQ -->|"process()"| CTRL
    CTRL -->|"soft_pwm_process()"| PWM
    CTRL -->|"fade_process()"| FADE
    PWM -->|"GPIO state"| LEDS
~~~

## Как читать схему

Схема состоит из нескольких уровней.

### 1. main

main создаёт локальный объект:

~~~ c
led_sequence_t sequence;
~~~

Этот объект содержит всё состояние верхнеуровневой LED-последовательности и вложенный экземпляр led_controller_t.

После создания объекта выполняется строго определённая последовательность:

~~~ text
leds_init()
    |
    v
nrfx_systick_init()
    |
    v
btn_init()
    |
    v
led_sequence_init()
~~~

Порядок является частью архитектурного контракта.

### 2. Кнопка

btn_init() поднимает системные зависимости кнопки, после чего btn_hw настраивает GPIOTE и debounce.

Физический путь:

~~~ text
BUTTON GPIO
     |
     v
GPIOTE edge
     |
     v
btn_hw debounce
     |
     v
confirmed state
     |
     v
btn callback
     |
     v
event queue
     |
     v
main
~~~

main не работает непосредственно с GPIO кнопки.

### 3. LED sequence

main регулярно передаёт sequence в:

~~~ c
led_sequence_process(&sequence);
~~~

led_sequence выбирает LED, определяет число циклов из DEVICE_ID, управляет межсветодиодной паузой и передаёт выполнение led_controller.

### 4. led_controller

led_controller отвечает за конкретный выбранный LED:

~~~ text
FADE_IN
   |
   v
FADE_OUT
   |
   +----> cycle pause ----+
   |                      |
   +----------------------+
   |
   v
next cycle / complete
~~~

После завершения всех циклов контроллер сообщает об этом led_sequence.

### 5. fade и soft_pwm

fade не управляет GPIO. Он рассчитывает очередную скважность и передаёт её в soft_pwm_set_duty().

soft_pwm преобразует скважность в физическую последовательность ON/OFF. При изменении логического состояния вызывается output callback. В текущей архитектуре callback — pwm_led_output() из led_controller.c.

### 6. leds

pwm_led_output() вызывает led_on() или led_off(). Модуль leds переводит логический индекс LED в GPIO конкретной платы.

Итоговый путь LED:

~~~ text
fade
  |
  | duty
  v
soft_pwm
  |
  | ON/OFF
  v
pwm_led_output()
  |
  v
leds
  |
  v
nrf_gpio_pin_write()
  |
  v
physical LED
~~~

---

# Инициализация приложения

## Общий порядок

Текущий main() выполняет:

~~~ c
leds_init();

nrfx_systick_init();

btn_init();

if (!led_sequence_init(
        &sequence,
        DEVICE_ID,
        PWM_PERIOD_US,
        FADE_DURATION_MS,
        CYCLE_PAUSE_MS,
        INTER_LED_PAUSE_MS)) while (true);
~~~

Каждый шаг подготавливает зависимости следующего уровня.

---

# 1. leds_init()

Первым инициализируется LED hardware.

Это необходимо потому, что глубоко вложенный soft_pwm_init() устанавливает физический PWM output в состояние OFF через callback.

Цепочка:

~~~ text
led_sequence_init()
    |
    v
led_controller_init()
    |
    v
soft_pwm_init()
    |
    v
pwm_led_output(false)
    |
    v
led_off()
    |
    v
GPIO
~~~

Поэтому GPIO светодиодов должен быть готов до создания led_controller.

leds_init():

1. настраивает GPIO всех LED как output;
2. переводит все LED в выключенное состояние.

---

# 2. nrfx_systick_init()

После LED hardware инициализируется общий SysTick.

SysTick используется несколькими независимыми объектами:

~~~ text
                 nrfx_systick
                      |
       +--------------+--------------+
       |              |              |
       v              v              v
   soft_pwm         fade      led_controller
                                      |
                                      v
                                led_sequence
~~~

В частности:

- soft_pwm использует SysTick для PWM phase timing;
- fade использует его для update interval;
- led_controller использует его для inter-cycle pause;
- led_sequence использует его для inter-LED pause.

SysTick является общим ресурсом приложения. Он инициализируется в main() ровно один раз. Ни один из перечисленных модулей не должен повторно вызывать nrfx_systick_init().

---

# 3. btn_init()

После SysTick инициализируется кнопочная подсистема.

Кнопка использует другой временной ресурс — LFCLK и app_timer.

Внутри btn_init():

~~~ text
btn_init()
    |
    +--> clock_init()
    |       |
    |       +--> nrf_drv_clock_init_check()
    |       +--> nrf_drv_clock_init() if needed
    |       +--> LFCLK request/start
    |
    +--> app_timer_init()
    |
    +--> btn_hw_init()
    |       |
    |       +--> app_timer_create(debounce)
    |       +--> nrfx_gpiote_init() if needed
    |       +--> nrfx_gpiote_in_init()
    |
    +--> app_timer_create(double press)
    |
    +--> btn_hw_start()
~~~

btn_init() владеет инициализацией общего legacy clock driver, LFCLK и app_timer.

btn_hw_init() предполагает, что LFCLK и app_timer уже готовы.

---

# 4. led_sequence_init()

Последним инициализируется верхний LED-уровень.

Он создаёт и инициализирует вложенный led_controller_t:

~~~ text
led_sequence_init()
       |
       v
led_controller_init()
       |
       +--> soft_pwm_init()
       |
       +--> fade_init()
~~~

При успешной инициализации устанавливаются:

~~~ text
led_idx = 0
prepared = false
running = false
inter_led_pause_state = NONE
inter_led_pause_elapsed_ms = 0
~~~

Первый LED не запускается автоматически.

Это намеренное поведение приложения: после старта устройства все LED остаются выключенными до первого BTN_EVENT_DOUBLE_PRESS.

---

# Fatal error при ошибке инициализации

led_sequence_init() возвращает bool.

При ошибке:

~~~ c
if (!led_sequence_init(
        &sequence,
        DEVICE_ID,
        PWM_PERIOD_US,
        FADE_DURATION_MS,
        CYCLE_PAUSE_MS,
        INTER_LED_PAUSE_MS)) while (true);
~~~

приложение остаётся в бесконечном цикле.

На текущем этапе это сознательно простая политика fatal error. Отдельной системы логирования или диагностики в main пока нет.

---

# Конфигурация приложения

Основные параметры находятся непосредственно в main.c:

~~~ c
#define DEVICE_ID               "6609"

#define PWM_PERIOD_US           1000
#define FADE_DURATION_MS        600
#define CYCLE_PAUSE_MS          500
#define INTER_LED_PAUSE_MS      500
~~~

Их роли:

| Параметр | Назначение | Владелец |
| --- | --- | --- |
| DEVICE_ID | Количество циклов каждого LED | led_sequence |
| PWM_PERIOD_US | Период software PWM | soft_pwm |
| FADE_DURATION_MS | Длительность fade-in/fade-out | fade / led_controller |
| CYCLE_PAUSE_MS | Пауза между циклами одного LED | led_controller |
| INTER_LED_PAUSE_MS | Пауза между разными LED | led_sequence |

main не интерпретирует эти значения самостоятельно. Он только передаёт их соответствующему модулю.

---

# DEVICE_ID

Текущая строка:

~~~ c
#define DEVICE_ID "6609"
~~~

используется led_sequence.

При текущем количестве LED каждый символ задаёт число циклов:

~~~ text
"6609"

LED 0 -> 6 cycles
LED 1 -> 6 cycles
LED 2 -> 0 cycles
LED 3 -> 9 cycles
~~~

main не разбирает строку. Проверка её минимальной длины также выполняется внутри led_sequence_init().

---

# Главный цикл

После успешной инициализации управление переходит в:

~~~ c
while (true) {
    ...
}
~~~

Внутри каждой итерации выполняются два основных этапа:

~~~ text
+--------------------------------+
|          main loop             |
+--------------------------------+
              |
              v
  led_sequence_process()
              |
              v
  btn_event_take() until empty
              |
              v
         next iteration
~~~

Порядок намеренный:

1. сначала обслуживается LED-подсистема;
2. затем обрабатываются все накопленные события кнопки.

---

# Обслуживание LED

Первый вызов:

~~~ c
led_sequence_process(&sequence);
~~~

должен выполняться на каждой итерации, включая состояние паузы.

Внутри:

~~~ text
led_sequence_process()
        |
        v
led_controller_process()
        |
        v
soft_pwm_process()
~~~

led_controller_process() сначала обслуживает PWM:

~~~ c
soft_pwm_process(&controller->pwm);
~~~

и только после этого проверяет, запущена ли последовательность.

Поэтому при sequence.running == false PWM всё равно продолжает получать сервис.

Это позволяет сохранять текущую среднюю яркость LED во время паузы.

Следовательно, main не должен заменять этот вызов условием:

~~~ c
if (led_sequence_is_running(&sequence))
    led_sequence_process(&sequence);
~~~

Такое изменение нарушило бы контракт нижнего уровня.

---

# Обработка событий кнопки

После LED service главный цикл извлекает события:

~~~ c
while ((event = btn_event_take()) != BTN_EVENT_NONE) {
    ...
}
~~~

Цикл продолжается до опустошения очереди. Поэтому за одну итерацию main может обработать несколько накопившихся событий.

---

# Какие события генерирует btn

Публичный тип:

~~~ c
typedef enum
{
    BTN_EVENT_NONE,
    BTN_EVENT_PRESS,
    BTN_EVENT_RELEASE,
    BTN_EVENT_SINGLE_PRESS,
    BTN_EVENT_DOUBLE_PRESS
} btn_event_t;
~~~

main использует только BTN_EVENT_DOUBLE_PRESS.

Остальные события извлекаются и игнорируются. Это часть общего интерфейса btn и позволяет использовать модуль в других приложениях.

---

# Почему main реагирует только на DOUBLE_PRESS

Для текущего приложения назначено одно действие:

~~~ text
DOUBLE_PRESS
     |
     v
toggle LED sequence
~~~

При:

~~~ c
if (led_sequence_is_running(&sequence))
    led_sequence_pause(&sequence);
else
    led_sequence_resume(&sequence);
~~~

получается:

~~~ text
running = false
      |
      | double press
      v
running = true
      |
      | double press
      v
running = false
      |
      | double press
      v
running = true
      |
     ...
~~~

Один подтверждённый double press является одной командой переключения состояния.

---

# Полный путь двойного нажатия

~~~ text
physical button
      |
      v
GPIO edge
      |
      v
GPIOTE interrupt
      |
      v
gpiote_pin_change_callback()
      |
      v
debounce timer
      |
      v
stable state
      |
      v
button_state_changed()
      |
      +--> PRESS / RELEASE
      +--> SINGLE_PRESS
      +--> DOUBLE_PRESS
              |
              v
        event queue
              |
              v
        btn_event_take()
              |
              v
             main
              |
              v
    led_sequence_pause/resume()
              |
              v
       LED sequence state
~~~

main не знает деталей debounce, GPIOTE или LFCLK.

---

# Формирование DOUBLE_PRESS

Логика double press находится в btn, а не в main.

После подтверждённого release:

1. текущий press-release цикл считается завершённым;
2. генерируется BTN_EVENT_SINGLE_PRESS;
3. запускается окно ожидания второго завершённого цикла;
4. если второй release происходит в пределах заданного интервала, добавляется BTN_EVENT_DOUBLE_PRESS.

main получает уже готовую логическую команду.

---

# Очередь событий

btn хранит события в кольцевой очереди фиксированного размера:

~~~ c
#define BTN_EVENT_QUEUE_SIZE 8
~~~

btn_event_take() извлекает одно событие за вызов.

Главный цикл вызывает его до получения BTN_EVENT_NONE.

При заполненной очереди новые события отбрасываются, чтобы сохранить уже накопленные непрочитанные события. Это решение полностью находится внутри btn.

---

# Взаимодействие с паузой

Есть два разных уровня паузы.

## Пауза LED sequence

main вызывает:

~~~ c
led_sequence_pause(&sequence);
~~~

Она может остановить:

- продолжение верхнеуровневой последовательности;
- текущий fade через led_controller_pause();
- или межсветодиодную паузу.

При этом PWM продолжает обслуживаться.

## Возобновление

main вызывает:

~~~ c
led_sequence_resume(&sequence);
~~~

Если текущий LED был подготовлен и находился в работе, его led_controller продолжает сохранённый fade.

Если последовательность находилась именно в межсветодиодной паузе, эта пауза начинается заново.

Если первый LED ещё не был подготовлен, он будет подготовлен следующим вызовом led_sequence_process().

---

# Начальное состояние приложения

После успешной инициализации:

~~~ text
LEDs             OFF
sequence.running FALSE
sequence.prepared FALSE
sequence.led_idx 0
controller       IDLE
~~~

Первый double press переводит:

~~~ text
running: FALSE -> TRUE
~~~

Но led_sequence_resume() не обязан немедленно подготовить LED 0.

Следующий вызов led_sequence_process() обнаружит prepared == false и выполнит подготовку LED 0.

---

# Нормальный жизненный цикл

~~~ text
BOOT
 |
 v
leds_init
 |
 v
SysTick init
 |
 v
button init
 |
 v
LED sequence init
 |
 v
IDLE / LEDs OFF
 |
 | double press
 v
RUNNING
 |
 v
prepare LED
 |
 v
fade-in
 |
 v
fade-out
 |
 +---- more cycles ----+
 |                     |
 +<-- cycle pause -----+
 |
 v
LED complete
 |
 v
next LED
 |
 v
inter-LED pause
 |
 v
prepare next LED
 |
 v
...
 |
 +--------------------------+
 |                          |
 +-------- wrap to LED 0 <--+
 |
 | double press
 v
PAUSED
 |
 | PWM still serviced
 |
 | double press
 v
RUNNING
~~~

После последнего LED приложение не завершается: последовательность циклически возвращается к LED 0.

---

# Временные ресурсы

В приложении используются два независимых механизма времени.

## SysTick

Используется LED-подсистемой:

~~~ text
SysTick
  |
  +--> soft_pwm
  +--> fade
  +--> led_controller
  +--> led_sequence
~~~

Он инициализируется один раз в main().

## LFCLK + app_timer

Используются кнопочной подсистемой:

~~~ text
LFCLK
  |
  v
app_timer
  |
  +--> debounce timer
  +--> double-press timer
~~~

Эта подсистема инициализируется внутри btn_init().

Механизмы времени не следует смешивать:

- SysTick обслуживает polling-логику LED;
- app_timer обслуживает debounce и double press.

---

# Почему main использует polling

Главный цикл не содержит WFE или другого длительного ожидания.

Причина — архитектура soft_pwm. PWM должен регулярно обслуживаться:

~~~ text
main loop
   |
   v
led_sequence_process()
   |
   v
led_controller_process()
   |
   v
soft_pwm_process()
~~~

Если главный цикл надолго блокируется, программный PWM перестанет переключаться в требуемые моменты.

Поэтому текущая архитектура сознательно использует непрерывный polling.

---

# Взаимодействие main и SysTick

main является владельцем инициализации SysTick, но не управляет его состоянием после запуска.

~~~ text
main
 |
 +--> nrfx_systick_init()
 |
 +--> regular processing
          |
          +--> soft_pwm
          +--> fade
          +--> led_controller
          +--> led_sequence
~~~

Ни один из этих модулей не должен повторно инициализировать SysTick.

---

# Взаимодействие main и LED callback

main не вызывает led_on() или led_off() в нормальном режиме.

Полный путь:

~~~ text
main
 |
 v
led_sequence
 |
 v
led_controller
 |
 +--> fade
 |
 +--> soft_pwm
        |
        v
   pwm_led_output()
        |
        v
      leds
~~~

Это сохраняет независимость верхнего уровня от GPIO.

---

# Ответственность main

main отвечает за:

- создание экземпляра led_sequence_t;
- конфигурацию приложения;
- порядок инициализации;
- инициализацию общего SysTick;
- запуск кнопочной подсистемы;
- запуск LED-последовательности;
- непрерывный polling;
- извлечение событий кнопки;
- преобразование BTN_EVENT_DOUBLE_PRESS в pause/resume.

main не отвечает за:

- debounce;
- GPIOTE;
- обработку физических GPIO кнопки;
- очередь событий;
- распознавание double press;
- выбор конкретного LED;
- чтение цифр DEVICE_ID;
- fade;
- PWM;
- GPIO LED;
- временные интервалы внутри нижних модулей.

---

# Границы ответственности

~~~ text
+------------------------------------------------------+
|                       main                           |
| configuration / initialization / application loop    |
+-----------------------------+------------------------+
                              |
             +----------------+----------------+
             |                                 |
             v                                 v
       button interface                 LED sequence interface
             |                                 |
             v                                 v
            btn                         led_sequence
             |                                 |
             v                                 v
          btn_hw                       led_controller
             |                           /       \
             v                          v         v
          GPIOTE                      fade     soft_pwm
             |                          |         |
             v                          +----+----+
       button GPIO                           |
                                             v
                                           leds
                                             |
                                             v
                                         LED GPIO
~~~

Каждый уровень скрывает детали нижнего уровня от непосредственного потребителя.

---

# Важные инварианты main

## SysTick инициализируется один раз

~~~ text
main
 |
 +--> nrfx_systick_init()
~~~

Повторной инициализации нет.

## LED hardware готов до soft_pwm

~~~ text
leds_init()
     |
     v
led_sequence_init()
     |
     v
led_controller_init()
     |
     v
soft_pwm_init()
~~~

## Кнопка полностью инициализируется до использования

~~~ text
btn_init()
     |
     v
main loop
     |
     v
btn_event_take()
~~~

## LED sequence process вызывается всегда

Даже когда последовательность остановлена:

~~~ text
running == false
       |
       v
led_sequence_process()
       |
       v
soft_pwm_process()
~~~

## Все события кнопки извлекаются

~~~ text
btn_event_take()
       |
       v
event != NONE ?
   |           |
  yes          no
   |           |
   v           v
process     next main loop
   |
   +--> repeat
~~~

---

# Поведение при нескольких событиях

Для двух завершённых press-release циклов очередь может содержать:

~~~ text
PRESS
RELEASE
SINGLE_PRESS
PRESS
RELEASE
SINGLE_PRESS
DOUBLE_PRESS
~~~

main извлечёт события по одному.

Для первых шести:

~~~ text
event != DOUBLE_PRESS
       |
       v
continue
~~~

Только последнее событие изменит состояние LED sequence.

Если очередь содержит несколько BTN_EVENT_DOUBLE_PRESS, каждое из них является отдельной командой:

~~~ text
DOUBLE_PRESS -> pause
DOUBLE_PRESS -> resume
DOUBLE_PRESS -> pause
~~~

main не объединяет такие события.

---

# Время жизни sequence

Объект:

~~~ c
led_sequence_t sequence;
~~~

является локальной переменной main().

Это безопасно, потому что main() никогда не возвращается.

DEVICE_ID является строковым литералом:

~~~ c
#define DEVICE_ID "6609"
~~~

и поэтому имеет статическое время жизни.

led_sequence хранит указатель на эту строку, а не копирует её.

---

# Сценарий паузы во время fade

~~~ text
main
 |
 v
led_sequence_process()
 |
 v
led_controller_process()
 |
 v
fade active
 |
 | double press
 v
main
 |
 v
led_sequence_pause()
 |
 v
led_controller_pause()
 |
 v
fade_stop()
 |
 v
sequence.running = false
 |
 +-------------------------------+
 | subsequent main loop          |
 |                               |
 | led_sequence_process()        |
 |       |                       |
 |       v                       |
 | soft_pwm_process()            |
 |       |                       |
 |       v                       |
 | current brightness preserved  |
 +-------------------------------+
 |
 | next double press
 v
led_sequence_resume()
 |
 v
led_controller_resume()
 |
 v
fade_resume()
 |
 v
fade continues
~~~

Пауза fade не теряет сохранённый прогресс.

---

# Сценарий паузы между LED

~~~ text
LED N complete
      |
      v
next_led()
      |
      v
inter-LED pause WAITING
      |
      | double press
      v
PAUSED
      |
      | main still calls process()
      |
      | double press
      v
WAITING
      |
      | full pause starts again
      v
LED N+1
~~~

В отличие от fade, межсветодиодная пауза не продолжается с прежнего значения.

---

# Сценарий завершения LED

Когда led_controller_process() возвращает true:

~~~ text
led_sequence_process()
        |
        v
controller completed
        |
        v
next_led()
        |
        v
prepared = false
        |
        v
inter-LED pause
        |
        v
prepare next LED
~~~

После последнего LED:

~~~ text
N-1 -> 0
~~~

Это не завершение приложения, а обычный wrap-around.

---

# Сценарий нулевого количества циклов

Если символ DEVICE_ID равен 0:

~~~ text
prepare_current_led()
       |
       v
blink_count = 0
       |
       v
next_led()
       |
       v
return
~~~

LED не запускается.

Для текущего DEVICE_ID:

~~~ text
"6609"

LED 0 -> 6
LED 1 -> 6
LED 2 -> skip
LED 3 -> 9
~~~

Если все символы равны нулю, приложение будет циклически пропускать LED.

---

# Сценарий нулевой inter-LED pause

Если INTER_LED_PAUSE_MS равен 0, временного ожидания нет.

Однако граница process сохраняется:

~~~ text
process N:
    complete LED
    next_led()
    return

process N+1:
    prepare next LED
~~~

Один вызов process() не должен проходить сразу несколько верхнеуровневых переходов.

---

# Сводная последовательность запуска LED

~~~ text
main
  |
  v
led_sequence_process
  |
  v
prepare_current_led
  |
  v
led_controller_prepare
  |
  v
led_controller_resume
  |
  v
start_fade_in
  |
  v
fade_start
  |
  v
soft_pwm_set_duty
  |
  v
soft_pwm_process
  |
  v
pwm_led_output
  |
  v
led_on
  |
  v
GPIO
  |
  v
LED
~~~

Последующие итерации продолжают этот процесс через
led_controller_process().

---

# Сводная последовательность обработки PWM

~~~ text
main loop
    |
    v
led_sequence_process()
    |
    v
led_controller_process()
    |
    +----------------------+
    |                      |
    v                      v
soft_pwm_process()     fade_process()
    |                      |
    |                      v
    |                 soft_pwm_set_duty()
    |                      |
    +----------+-----------+
               |
               v
         soft_pwm state
               |
               v
      pwm_led_output()
               |
               v
             leds
               |
               v
             GPIO
~~~

Важная особенность: soft_pwm_process() вызывается до fade_process(). Поэтому PWM обслуживается даже тогда, когда fade или controller находятся в паузе.

---

# Сводная последовательность обработки кнопки

~~~ text
button GPIO
    |
    v
GPIOTE
    |
    v
debounce
    |
    v
confirmed state
    |
    v
btn callback
    |
    v
event queue
    |
    v
main: btn_event_take()
    |
    v
DOUBLE_PRESS?
   /        \
 no          yes
 |            |
 v            v
ignore    is_running()
             |
       +-----+-----+
       |           |
      true        false
       |           |
       v           v
    pause()     resume()
       |           |
       +-----+-----+
             |
             v
       LED sequence
~~~

---

# Роль main как composition root

Архитектурно main является composition root приложения.

Именно здесь:

- выбираются конкретные параметры приложения;
- создаются верхнеуровневые объекты;
- устанавливается порядок инициализации;
- соединяются независимые подсистемы;
- определяется реакция приложения на пользовательское событие.

При этом алгоритмы самих подсистем остаются внутри соответствующих модулей.

Поэтому main.c следует рассматривать не как место реализации LED-логики или button-логики, а как описание того, как готовые компоненты работают вместе.

---

# Итоговая модель

~~~ text
                           +----------------+
                           |     main()      |
                           +--------+-------+
                                    |
                  +-----------------+-----------------+
                  |                                   |
                  v                                   v
         +----------------+                  +-------------------+
         |   btn / hw     |                  |   led_sequence    |
         +-------+--------+                  +---------+---------+
                 |                                     |
          GPIOTE / timer                         device_id / index
                 |                                     |
                 v                                     v
         BUTTON GPIO                         +-------------------+
                                             |  led_controller   |
                                             +----+----------+---+
                                                  |          |
                                                  v          v
                                               fade      soft_pwm
                                                  |          |
                                                  +-----+----+
                                                        |
                                                        v
                                                 output callback
                                                        |
                                                        v
                                                     leds
                                                        |
                                                        v
                                                   LED GPIO
                                                        |
                                                        v
                                                  physical LED

Shared timing:
    main -> nrfx_systick_init()
                  |
                  +--> soft_pwm
                  +--> fade
                  +--> led_controller
                  +--> led_sequence

Button timing:
    btn -> LFCLK -> app_timer
                  |
                  +--> debounce
                  +--> double press
~~~

Ключевые правила:

- main инициализирует SysTick ровно один раз;
- LED hardware готовится до инициализации soft_pwm;
- кнопочная подсистема сама владеет LFCLK и app_timer;
- main не обращается к нижним LED-модулям напрямую;
- main не обращается к GPIO кнопки напрямую;
- led_sequence_process() вызывается постоянно, включая паузу;
- btn_event_take() вызывается до полного опустошения очереди;
- только BTN_EVENT_DOUBLE_PRESS меняет состояние LED sequence;
- PWM продолжает обслуживаться во время верхнеуровневой паузы;
- первый double press запускает последовательность после boot;
- последующие double press переключают pause/resume;
- после последнего LED последовательность возвращается к LED 0;
- main() не завершается.
