# Logging

Модуль скрывает Nordic logger и USB CDC ACM backend от остального приложения.

## Состав

- [log.md](log.md) — API, инициализация, deferred processing и ограничения.
- `log.h` / `log.c` — интерфейс и реализация тонкой обёртки.

## Публичный интерфейс

```c
void log_init(void);
void log_process(void);

LOG_INFO(...);
LOG_WARNING(...);
LOG_ERROR(...);
```

`main()` вызывает `log_init()` при запуске и регулярно вызывает `log_process()` в основном цикле. Вызовы `LOG_*` используют deferred logger; USB events и ожидающие сообщения обрабатываются из main loop.

Модуль не инициализирует LFCLK или app_timer. Эти ресурсы подготавливает [timer_service](../timer_service/timer_service.md).
