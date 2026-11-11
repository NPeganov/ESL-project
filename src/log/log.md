# log

Модуль log — тонкая прикладная обёртка над Nordic logger.

Его задача — скрыть от application-кода зависимости от Nordic logging API и USB backend.

---

# Публичный API

~~~ c
void log_init(void);
void log_process(void);

#define LOG_INFO(...)
#define LOG_WARNING(...)
#define LOG_ERROR(...)
~~~

Макросы LOG_INFO(), LOG_WARNING() и LOG_ERROR() являются прямыми обёртками соответствующих макросов Nordic logger.

Модуль не предоставляет собственного формата сообщений, фильтрации или буферизации.

---

# Инициализация

`log_init()` выполняет:

~~~ text
log_init()
    |
    +--> NRF_LOG_INIT(NULL)
    |
    +--> NRF_LOG_DEFAULT_BACKENDS_INIT()
              |
              v
       USB logging backend
              |
              v
          USB CDC ACM
~~~

USB backend инициализируется через стандартный механизм NRF_LOG_DEFAULT_BACKENDS_INIT().

Отдельный вызов USB backend init из application-кода не требуется.

---

# Временная зависимость

USB backend использует clock infrastructure.

В текущей архитектуре владельцем shared legacy clock driver и LFCLK является btn_init():

~~~ text
btn_init()
    |
    +--> nrf_drv_clock_init_check()
    +--> nrf_drv_clock_init() if needed
    +--> LFCLK request/start
    +--> app_timer_init()
              |
              +-------------------+
                                  |
                                  v
                           log USB backend
~~~

Поэтому log_init() вызывается после btn_init().

Модуль log не должен самостоятельно инициализировать clock driver или LFCLK.

---

# Обработка

Главный цикл вызывает:

~~~ c
log_process();
~~~

Внутри:

~~~ text
log_process()
    |
    +--> LOG_BACKEND_USB_PROCESS()
    |          |
    |          v
    |     app_usbd event queue
    |
    +--> NRF_LOG_PROCESS()
               |
               v
        one deferred log entry
~~~

NRF_LOG_PROCESS() в deferred mode обрабатывает максимум одну запись за вызов.

Поэтому log_process() вызывается регулярно из main loop.

USB event processing также выполняется каждый проход main loop.

---

# Особенность USB передачи

USB backend может синхронно ожидать завершения передачи при отправке сообщения в открытый CDC порт.

Поэтому deferred logging уменьшает работу в месте вызова LOG_*, но не делает обработку log_process() гарантированно неблокирующей.

Это особенно важно для текущего polling-based software PWM: длительная USB передача может увеличить интервал между вызовами soft_pwm_process().

На этом этапе logging не используется из ISR.

---

# Граница модуля

Application-код:

~~~ c
#include "log/log.h"

log_init();

LOG_INFO("Application started");

log_process();
~~~

не должен знать о Nordic logger frontend, default backends, USB backend, app_usbd или CDC ACM.

Эти детали остаются внутри реализации и конфигурации модуля.

---

# Конфигурация

USB logging backend включается через app_config.h.

Основные параметры:

~~~ c
#define NRF_LOG_ENABLED 1
#define NRF_LOG_DEFERRED 1
#define LOG_BACKEND_USB_ENABLED 1
#define APP_USBD_ENABLED 1
~~~

Полная конфигурация backend и USB device находится в src/app_config.h.

src/sdk_config.h остаётся общей SDK-конфигурацией и подключает app_config.h при определённом USE_APP_CONFIG.

---

# Ограничения

Модуль log не должен:

- владеть LFCLK;
- владеть app_timer;
- инициализировать USB из main напрямую;
- содержать application-specific log policy;
- выполнять тяжёлую обработку в ISR.

Его роль — минимальный composition boundary между application и Nordic logging stack.