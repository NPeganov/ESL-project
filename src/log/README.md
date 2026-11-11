# Logging

Модуль логирования скрывает детали Nordic logger и USB CDC backend от остального приложения.

## Состав

- [log.md](log.md) — подробное описание API, инициализации и обработки отложенных сообщений;
- log.h / log.c — публичный интерфейс и реализация тонкой обёртки.

## Публичный интерфейс

~~~ c
void log_init(void);
void log_process(void);

LOG_INFO(...)
LOG_WARNING(...)
LOG_ERROR(...)
~~~

Внешний код не должен напрямую использовать Nordic logger или USB backend.

## Архитектура

~~~ text
main
 |
 +--> log_init()
 |       |
 |       +--> nrf_log
 |       |
 |       +--> USB backend
 |                |
 |                v
 |             USB CDC ACM
 |
 +--> log_process()
         |
         +--> USB event queue
         |
         +--> deferred log queue
~~~

Модуль не владеет clock infrastructure. LFCLK и legacy clock driver подготавливаются btn_init(), после чего используются USB logging backend.