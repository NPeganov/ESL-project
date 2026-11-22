# log — логирование приложения

## Назначение

`log` — тонкая обёртка над Nordic logger и включённым USB CDC ACM backend. Она предоставляет приложению единый интерфейс и скрывает детали Nordic logging API.

## Публичный API

```c
void log_init(void);
void log_process(void);

LOG_INFO(...);
LOG_WARNING(...);
LOG_ERROR(...);
```

Макросы напрямую вызывают соответствующие макросы Nordic logger. Модуль не реализует собственную очередь, форматирование или политику уровней.

## Инициализация

`log_init()` вызывает:

1. `NRF_LOG_INIT(NULL)`;
2. `NRF_LOG_DEFAULT_BACKENDS_INIT()`.

Второй вызов инициализирует настроенные backend, включая USB logging backend.

В текущем `main()` логирование инициализируется до `hsv_picker_init()`. Это не инициализирует LFCLK или app_timer: общую timer infrastructure позднее подготавливает `timer_service_init()`.

## Обработка отложенных сообщений

В конфигурации включён deferred logging. Макросы `LOG_INFO`, `LOG_WARNING` и `LOG_ERROR` передают сообщения Nordic logger; фактическая обработка выполняется из основного цикла.

`log_process()` сначала вызывает `LOG_BACKEND_USB_PROCESS()`, затем `NRF_LOG_PROCESS()`. Поэтому функция должна регулярно вызываться из main loop.

USB backend может отбросить сообщения, пока CDC ACM порт не открыт. Это отражено в реализации комментарием перед обработкой USB events.

## Использование в callback

Прикладные callback кнопки могут записывать сообщения через deferred logger. USB обработка не вызывается непосредственно макросом `LOG_*`; отправка и обработка backend выполняются позднее из `log_process()`.

Callback всё равно должен оставаться коротким и не выполнять синхронную USB-передачу или тяжёлую работу.

## Конфигурация

Основные параметры находятся в `src/config/app_config.h` и SDK configuration:

- `NRF_LOG_ENABLED`;
- `NRF_LOG_DEFERRED`;
- `LOG_BACKEND_USB_ENABLED`;
- `APP_USBD_ENABLED`;
- `APP_USBD_CDC_ACM_ENABLED`.

## Границы ответственности

`log` не владеет LFCLK, app_timer или прикладными таймерами. Он не выбирает, какие события приложения логировать, и не содержит application-specific сообщений. Точка вызова `log_process()` принадлежит основному циклу в `main`.
