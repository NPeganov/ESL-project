# led_sequence

Модуль led_sequence является верхнеуровневым координатором последовательности
работы светодиодов. Он выбирает LED по логическому индексу, получает из
device_id количество циклов для выбранного LED и передаёт эту информацию
в led_controller.

Модуль не управляет GPIO, не формирует PWM и не рассчитывает яркость.
Эти задачи выполняют leds, soft_pwm и fade через led_controller.

Основные свойства:

- один экземпляр управляет последовательностью всех доступных LED;
- количество циклов каждого LED задаётся одним символом device_id;
- цифра 0 означает пропуск LED;
- нецифровой символ также означает нулевое количество циклов;
- после последнего LED индекс возвращается к нулю;
- между разными LED может выполняться отдельная пауза;
- межсветодиодная пауза при возобновлении начинается заново;
- нулевая межсветодиодная пауза не создаёт временной задержки;
- следующий LED не подготавливается в том же process-вызове, в котором
  завершился предыдущий;
- пауза верхнего уровня не прекращает обслуживание PWM;
- SysTick используется как общий источник времени и не инициализируется
  этим модулем.

---

# Место в архитектуре

~~~ text
application
    |
    v
led_sequence
    |
    v
led_controller
    |
    +-------------------------+
    |                         |
    v                         v
  fade                     soft_pwm
    |                         |
    | set duty                | output callback
    +------------------------>+
                              |
                              v
                       pwm_led_output()
                              |
                              v
                            leds
                              |
                              v
                             GPIO
                              |
                              v
                         physical LED
~~~

Ответственность модулей:

| Модуль | Ответственность |
| --- | --- |
| led_sequence | Выбор LED, количество циклов, переходы между LED и межсветодиодные паузы |
| led_controller | Циклы одного LED, fade-in/fade-out и межцикловая пауза |
| fade | Расчёт промежуточной скважности |
| soft_pwm | Формирование PWM ON/OFF |
| leds | Управление физическими GPIO |

led_sequence знает, какой LED и сколько циклов нужно выполнить.
led_controller знает, как выполнить эти циклы.

---

# Общая модель

Внешний код передаёт строку device_id:

~~~ text
device_id = "2510"

LED 0 -> 2 cycles
LED 1 -> 5 cycles
LED 2 -> 1 cycle
LED 3 -> 0 cycles
~~~

Для каждого LED выполняется следующий алгоритм:

1. взять символ device_id с индексом текущего LED;
2. проверить, является ли символ цифрой;
3. преобразовать цифру в число от 0 до 9;
4. при нуле пропустить LED;
5. при ненулевом значении вызвать led_controller_prepare();
6. запустить подготовленный контроллер;
7. регулярно обслуживать его через led_sequence_process();
8. после завершения контроллера перейти к следующему LED;
9. выдержать межсветодиодную паузу;
10. повторить цикл.

После LED с индексом LEDS_NUMBER - 1 индекс становится 0.

~~~ text
LED 0 -> LED 1 -> LED 2 -> ... -> LED N-1
  ^                                      |
  |______________________________________|
~~~

Последовательность поэтому является бесконечной.

---

# Источник количества циклов

Основным входом является device_id.

В структуре хранится только указатель:

~~~ c
const char *device_id;
~~~

Копия строки не создаётся.

Для текущего LED используется:

~~~ c
sequence->device_id[sequence->led_idx]
~~~

Проверка выполняется через isdigit():

~~~ c
if (isdigit((unsigned char)sequence->device_id[sequence->led_idx]))
    blink_count = (uint8_t)(sequence->device_id[sequence->led_idx] - '0');
~~~

Получаемая таблица:

| Символ | Циклы |
| --- | ---: |
| 0 | 0 |
| 1 | 1 |
| 2 | 2 |
| ... | ... |
| 9 | 9 |
| A | 0 |
| пробел | 0 |
| - | 0 |

Ненулевым считается только десятичный символ.

Приведение к unsigned char перед isdigit() необходимо для корректного
использования стандартной функции при реализации, где char является
знаковым типом.

---

# Требования к device_id

При инициализации проверяется:

~~~ c
strlen(device_id) >= LEDS_NUMBER
~~~

Строка должна содержать минимум один символ для каждого LED.

Строка может быть длиннее LEDS_NUMBER. Дополнительные символы
не используются.

Поскольку сохраняется указатель, память строки должна существовать
всё время работы экземпляра. Строковый литерал является безопасным
вариантом:

~~~ c
#define DEVICE_ID "6609"
~~~

Локальный массив, который уничтожается после выхода из функции,
передавать нельзя.

Изменение содержимого строки во время работы также не является частью
текущего контракта.

---

# Структура led_sequence_t

~~~ c
typedef struct {
    led_controller_t controller;
    const char *device_id;   // Device ID used to determine blink counts
    uint8_t led_idx;         // Current LED index
    bool prepared;           // Current LED has been prepared
    bool running;            // LED sequence is active

    inter_led_pause_state_t inter_led_pause_state;
    nrfx_systick_state_t inter_led_pause_start;
    uint32_t inter_led_pause_elapsed_ms;
    uint32_t inter_led_pause_duration_ms;
} led_sequence_t;
~~~

Назначение полей:

| Поле | Назначение |
| --- | --- |
| controller | Единственный вложенный led_controller |
| device_id | Источник количества циклов |
| led_idx | Текущий логический индекс LED |
| prepared | Текущий LED подготовлен в controller |
| running | Верхнеуровневая последовательность активна |
| inter_led_pause_state | Состояние межсветодиодной паузы |
| inter_led_pause_start | Начальная отметка SysTick |
| inter_led_pause_elapsed_ms | Накопленное время паузы |
| inter_led_pause_duration_ms | Заданная длительность паузы |

Всё состояние является локальным для экземпляра структуры.

---

# Состояния межсветодиодной паузы

Используется:

~~~ c
typedef enum {
    INTER_LED_PAUSE_NONE,
    INTER_LED_PAUSE_WAITING,
    INTER_LED_PAUSE_PAUSED
} inter_led_pause_state_t;
~~~

## NONE

Пауза отсутствует.

Это начальное состояние, состояние после завершения паузы и состояние
при нулевой длительности.

## WAITING

Пауза выполняется.

В этом состоянии inter_led_pause_process() периодически увеличивает
inter_led_pause_elapsed_ms.

## PAUSED

Последовательность приостановлена именно во время межсветодиодной
паузы.

При resume() эта пауза начинается заново, а не продолжается с
сохранённого значения elapsed_ms.

---

# Инициализация

Публичная функция:

~~~ c
bool led_sequence_init(
    led_sequence_t *sequence,
    const char *device_id,
    uint32_t pwm_period_us,
    uint32_t fade_duration_ms,
    uint32_t cycle_pause_ms,
    uint32_t inter_led_pause_ms);
~~~

Проверяются:

~~~ c
sequence == NULL
device_id == NULL
strlen(device_id) < LEDS_NUMBER
~~~

При любой такой ошибке возвращается false.

Затем вызывается led_controller_init().

Если вложенный контроллер не инициализировался, функция также
возвращает false.

После успешной инициализации устанавливается:

~~~ text
led_idx = 0
prepared = false
running = false
inter_led_pause_state = NONE
inter_led_pause_elapsed_ms = 0
inter_led_pause_duration_ms = inter_led_pause_ms
~~~

Первый LED ещё не подготовлен.

Инициализация SysTick здесь не выполняется.

---

# Начальный жизненный цикл

Обычный порядок работы:

~~~ text
leds_init()
     |
     v
nrfx_systick_init()
     |
     v
led_sequence_init()
     |
     v
led_sequence_resume()
     |
     v
led_sequence_process()
     |
     v
periodic processing
~~~

SysTick должен быть инициализирован верхним уровнем приложения один раз
до использования потребителями времени.

led_sequence не должен повторно вызывать nrfx_systick_init().

---

# Запуск

led_sequence_resume() запускает последовательность.

~~~ c
void led_sequence_resume(led_sequence_t *sequence);
~~~

Если sequence == NULL или running уже равен true, функция ничего
не делает.

Если текущая межсветодиодная пауза находится в PAUSED, она запускается
заново.

Если текущий LED уже подготовлен, вызывается:

~~~ c
led_controller_resume(&sequence->controller);
~~~

Если LED ещё не подготовлен, его подготовка произойдёт через
следующий led_sequence_process().

В конце:

~~~ c
sequence->running = true;
~~~

resume() сам не выполняет prepare_current_led(). Это сохраняет единый
polling-контракт.

---

# Приостановка

led_sequence_pause() работает только с активной последовательностью.

~~~ c
void led_sequence_pause(led_sequence_t *sequence);
~~~

При NULL или running == false функция сразу возвращается.

Если активна межсветодиодная пауза:

~~~ c
inter_led_pause_pause(sequence);
~~~

состояние меняется WAITING -> PAUSED.

Если текущий LED подготовлен:

~~~ c
led_controller_pause(&sequence->controller);
~~~

После этого:

~~~ c
sequence->running = false;
~~~

---

# Важное свойство паузы: PWM продолжает работать

led_sequence_process() сначала вызывает:

~~~ c
sequence_completed =
    led_controller_process(&sequence->controller);
~~~

и только затем проверяет running.

Поэтому даже при:

~~~ c
sequence->running == false
~~~

вложенный led_controller успевает обслужить soft_pwm.

Это необходимо, чтобы промежуточная яркость сохранялась физическим
PWM во время паузы.

Иначе остановка верхнего уровня могла бы остановить PWM-фронты.

---

# Основной обработчик

~~~ c
void led_sequence_process(led_sequence_t *sequence);
~~~

Порядок работы:

1. проверить sequence на NULL;
2. вызвать led_controller_process();
3. если running == false, выйти;
4. если текущий LED завершён, выбрать следующий;
5. запустить межсветодиодную паузу;
6. обслужить паузу, если она активна;
7. если текущий LED не подготовлен, подготовить его.

Первый шаг обработки намеренно выполняется даже при остановленной
верхнеуровневой последовательности.

---

# Завершение текущего LED

Когда led_controller_process() возвращает true, текущий LED завершил
все свои циклы.

Выполняется:

~~~ c
next_led(sequence);
sequence->prepared = false;
inter_led_pause_start(sequence);
~~~

Это означает:

1. выбран следующий LED;
2. старый LED больше не считается подготовленным;
3. начинается переход между LED.

Сам led_sequence не выполняет повторную подготовку старого LED.

---

# Выбор следующего LED

Функция:

~~~ c
static void next_led(led_sequence_t *sequence) {
    ++sequence->led_idx;

    if (sequence->led_idx >= LEDS_NUMBER)
        sequence->led_idx = 0;
}
~~~

Переход:

~~~ text
0 -> 1 -> 2 -> ... -> N-1 -> 0
~~~

После последнего LED нет конечного состояния.

Проверка выхода за пределы выполняется после увеличения индекса.

---

# Нулевая межсветодиодная пауза

Если:

~~~ c
sequence->inter_led_pause_duration_ms == 0
~~~

inter_led_pause_start() сразу устанавливает:

~~~ c
sequence->inter_led_pause_state = INTER_LED_PAUSE_NONE;
~~~

SysTick для этой паузы не запускается.

Но следующий LED не подготавливается в том же вызове process().

После:

~~~ c
next_led(sequence);
sequence->prepared = false;
inter_led_pause_start(sequence);
~~~

обработчик возвращается.

Следующий LED будет подготовлен следующим вызовом.

Это принципиально сохраняет правило: один вызов process() не проходит
через несколько верхнеуровневых LED-переходов.

---

# Ненулевая межсветодиодная пауза

При длительности больше нуля inter_led_pause_start():

1. обнуляет elapsed_ms;
2. получает текущую отметку SysTick;
3. переводит состояние в WAITING.

Далее process() вызывает inter_led_pause_process().

Если пауза ещё не закончилась, функция возвращает false и обработчик
завершается.

Если пауза завершена, состояние становится NONE, после чего может быть
подготовлен следующий LED.

---

# Временная модель паузы

Константа:

~~~ c
#define INTER_LED_PAUSE_UPDATE_INTERVAL_MS   1
~~~

задаёт шаг накопления времени.

Проверка:

~~~ c
if (!nrfx_systick_test(
        &sequence->inter_led_pause_start,
        INTER_LED_PAUSE_UPDATE_INTERVAL_MS * 1000))
    return false;
~~~

При каждом успешном интервале:

~~~ c
nrfx_systick_get(&sequence->inter_led_pause_start);
sequence->inter_led_pause_elapsed_ms +=
    INTER_LED_PAUSE_UPDATE_INTERVAL_MS;
~~~

Когда накопленное время достигает заданной длительности:

~~~ c
sequence->inter_led_pause_state = INTER_LED_PAUSE_NONE;
sequence->inter_led_pause_elapsed_ms = 0;
~~~

и функция возвращает true.

За один вызов выполняется максимум один шаг.

Следовательно, пропущенные вызовы не компенсируются. При редком
вызове process() фактическая пауза может быть длиннее заданной.

---

# Возобновление межсветодиодной паузы

При состоянии PAUSED resume() вызывает:

~~~ c
inter_led_pause_resume(sequence);
~~~

а эта функция вызывает:

~~~ c
inter_led_pause_start(sequence);
~~~

Поэтому:

- elapsed_ms обнуляется;
- новый стартовый timestamp берётся заново;
- полная длительность паузы отсчитывается сначала.

Это отличается от fade-паузы, где нижележащий fade сохраняет свой
прогресс.

---

# Подготовка текущего LED

Функция:

~~~ c
static void prepare_current_led(led_sequence_t *sequence);
~~~

начинает с:

~~~ c
uint8_t blink_count = 0;
~~~

Если текущий символ является цифрой, значение преобразуется.

При нуле:

~~~ c
next_led(sequence);
return;
~~~

При ненулевом значении:

~~~ c
led_controller_prepare(
    &sequence->controller,
    sequence->led_idx,
    blink_count);

sequence->prepared = true;
led_controller_resume(&sequence->controller);
~~~

Таким образом, только led_sequence определяет выбор LED и число циклов.
Сама реализация циклов остаётся ответственностью led_controller.

---

# Пропуск LED с нулевым количеством циклов

Нулевой LED не запускается.

Например:

~~~ text
device_id = "2003"

LED 0 -> 2 cycles
LED 1 -> 0 cycles
LED 2 -> 0 cycles
LED 3 -> 3 cycles
~~~

После LED 0 последовательность может пройти:

~~~ text
LED 1 -> skip
LED 2 -> skip
LED 3 -> start
~~~

Каждый такой переход выполняется в отдельной итерации process().

Модуль не выполняет потенциально длинную цепочку пропусков за один
вызов.

Если все LED имеют нулевое количество циклов, последовательность
будет циклически пропускать их. В текущем API это допустимое состояние.

---

# Взаимодействие с led_controller

В структуре находится один экземпляр:

~~~ c
led_controller_t controller;
~~~

Он переиспользуется для всех LED.

При выборе нового LED вызывается:

~~~ c
led_controller_prepare(
    &sequence->controller,
    sequence->led_idx,
    blink_count);
~~~

led_sequence не знает внутренних фаз контроллера.

Для верхнего уровня его контракт состоит из:

- prepare();
- resume();
- pause();
- process();
- результата process(), сообщающего о завершении.

---

# Два независимых уровня пауз

Очень важно не смешивать:

~~~ c
cycle_pause_ms
~~~

и:

~~~ c
inter_led_pause_ms
~~~

Первый параметр принадлежит led_controller и задаёт паузу между
циклами одного LED.

Второй принадлежит led_sequence и задаёт паузу между разными LED.

~~~ text
LED N
 |
 +-- fade-in
 +-- fade-out
 +-- cycle pause
 +-- fade-in
 +-- fade-out
 |
 +-- completed
       |
       +-- inter-LED pause
       |
       v
     LED N+1
~~~

Эти интервалы имеют разные владельцы и разные состояния.

---

# Взаимодействие с fade и soft_pwm

Прямой зависимости led_sequence от fade и soft_pwm нет.

Полная цепочка:

~~~ text
led_sequence_process()
        |
        v
led_controller_process()
        |
        +--> soft_pwm_process()
        |
        +--> fade_process()
        |       |
        |       v
        |   soft_pwm_set_duty()
        |
        v
   PWM output callback
        |
        v
       leds
~~~

led_sequence управляет только верхним уровнем.

Особенно важно, что soft_pwm продолжает обслуживаться при
приостановленной последовательности.

---

# SysTick

led_sequence использует общий SysTick только для межсветодиодной
паузы.

Он не вызывает nrfx_systick_init().

Инициализация принадлежит верхнему уровню приложения:

~~~ text
main()
   |
   v
nrfx_systick_init()
   |
   +-----------------------------+
   |                             |
   v                             v
led_sequence                 led_controller
                                 |
                       +---------+---------+
                       |                   |
                       v                   v
                     fade              soft_pwm
~~~

Повторная инициализация SysTick внутри led_sequence недопустима,
поскольку это общий временной источник для нескольких модулей.

---

# Публичный API

## led_sequence_init()

Инициализирует экземпляр.

Возвращает false при:

- NULL sequence;
- NULL device_id;
- слишком коротком device_id;
- ошибке led_controller_init().

Не запускает последовательность.

## led_sequence_pause()

Приостанавливает активную последовательность.

При паузе между LED переводит собственное состояние в PAUSED.
При работе LED передаёт паузу led_controller.

## led_sequence_resume()

Запускает или возобновляет последовательность.

При PAUSED межсветодиодной паузе она начинается заново.
Подготовленный LED возобновляется через led_controller.

## led_sequence_process()

Главная polling-функция.

Она обслуживает led_controller, переходы между LED, межсветодиодную
паузу и подготовку следующего LED.

## led_sequence_is_running()

Возвращает true только при корректном активном экземпляре.
Для NULL возвращает false.

---

# Правила использования

Приложение должно регулярно вызывать:

~~~ c
led_sequence_process(&sequence);
~~~

Никакой собственной периодической задачи или аппаратного таймера
для process() модуль не создаёт.

До led_sequence_init() должны быть готовы нижележащие зависимости,
включая GPIO LED и общий SysTick.

Функции не защищены от конкурентного доступа. Вызовы должны
выполняться из одного согласованного контекста либо с внешней
синхронизацией.

---

# Типовые сценарии

## Обычный запуск

~~~ text
init
 |
 v
resume
 |
 v
process
 |
 v
prepare LED 0
 |
 v
controller runs LED 0
 |
 v
controller complete
 |
 v
next LED
 |
 v
inter-LED pause
 |
 v
prepare LED 1
 |
 v
...
~~~

## Пауза во время LED

~~~ text
LED N running
      |
      v
pause()
      |
      +--> led_controller_pause()
      |
      v
running = false
      |
      v
process()
      |
      +--> PWM still processed
      |
      +--> sequence progression stopped
      |
      v
resume()
      |
      v
led_controller_resume()
      |
      v
LED N continues
~~~

## Пауза между LED

~~~ text
LED N complete
      |
      v
next_led()
      |
      v
WAITING
      |
      v
pause()
      |
      v
PAUSED
      |
      v
resume()
      |
      v
WAITING
      |
      | full interval starts again
      v
next LED
~~~

## Нулевая пауза

~~~ text
LED N complete
      |
      v
next_led()
      |
      v
duration == 0
      |
      v
NONE
      |
      v
return
      |
      v
next process()
      |
      v
prepare next LED
~~~

## Последний LED

~~~ text
LED N-1 complete
      |
      v
next_led()
      |
      v
index reaches LEDS_NUMBER
      |
      v
index = 0
      |
      v
LED 0
~~~

Это не завершение всей последовательности, а обычный wrap-around.

---

# Один шаг за один process()

Модуль сознательно сохраняет границы обработки.

За один вызов process() не следует пытаться:

- выполнить несколько LED;
- одновременно завершить несколько временных интервалов;
- пройти всю цепочку нулевых LED.

Это особенно важно для нулевой межсветодиодной паузы: она означает
отсутствие времени ожидания, но не разрешает подготовить следующий
LED в том же вызове.

Такое поведение делает нагрузку одной итерации предсказуемой.

---

# Временная схема всей системы

Для одного LED с несколькими циклами:

~~~ text
             led_sequence
                  |
                  v
            prepare LED N
                  |
                  v
          +---------------+
          | led_controller|
          +---------------+
                  |
        +---------+---------+
        |                   |
        v                   v
     fade-in             fade-out
        |                   |
        +---------+---------+
                  |
             more cycles?
              /       \
            yes        no
             |          |
             v          v
       cycle pause   LED complete
                         |
                         v
                  next_led()
                         |
                         v
                 inter-LED pause
                         |
                         v
                    next LED
~~~

Таким образом существуют два уровня цикличности:

1. циклы одного LED внутри led_controller;
2. циклический обход всех LED внутри led_sequence.

---

# Защита NULL

Все публичные функции имеют защиту от NULL.

Для led_sequence_init():

~~~ c
if (sequence == NULL || device_id == NULL || strlen(device_id) < LEDS_NUMBER)
    return false;
~~~

Для pause(), resume() и process() используется ранний выход.

Для is_running():

~~~ c
if (sequence == NULL)
    return false;
~~~

Проверка NULL защищает API от некорректного указателя, но не делает
валидным указатель на неинициализированную или уже освобождённую
структуру.

---

# Ограничения

## Один активный LED

Одновременно работает только один led_controller.

Параллельное плавное управление несколькими LED этим модулем
не поддерживается.

## Polling

Временная модель зависит от регулярности process().

Редкие вызовы могут:

- увеличить фактическую межсветодиодную паузу;
- задержать подготовку следующего LED;
- увеличить задержку между логическими переходами.

## Время жизни device_id

Строка должна существовать и оставаться валидной во время работы
экземпляра.

## Общий SysTick

SysTick является общим ресурсом приложения и должен инициализироваться
один раз.

---

# Итоговая модель

~~~ text
                 device_id
                     |
                     v
              +--------------+
              | led_sequence |
              +--------------+
                     |
          select LED + cycle count
                     |
                     v
             +---------------+
             |led_controller |
             +---------------+
                     |
          +----------+----------+
          |                     |
          v                     v
        fade                 soft_pwm
          |                     |
          +----------+----------+
                     |
                     v
                   leds
                     |
                     v
                    GPIO
~~~

Ключевые правила:

- device_id определяет количество циклов каждого LED;
- нулевые и нецифровые значения пропускаются;
- одновременно выполняется только один LED;
- после последнего LED индекс возвращается к нулю;
- межсветодиодная пауза принадлежит led_sequence;
- межцикловая пауза принадлежит led_controller;
- нулевая межсветодиодная пауза не создаёт временной задержки;
- следующий LED при этом подготавливается только следующим вызовом
  process();
- пауза верхнего уровня не останавливает PWM;
- возобновление межсветодиодной паузы начинает её заново;
- SysTick инициализируется один раз на уровне приложения;
- led_sequence не управляет GPIO и не реализует PWM или fade.
