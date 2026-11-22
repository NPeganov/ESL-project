# Button subsystem

Подсистема кнопки состоит из двух уровней:

- [btn_hw.md](btn_hw.md) — GPIO/GPIOTE и debounce;
- [btn.md](btn.md) — логические PRESS/RELEASE/SINGLE_PRESS/DOUBLE_PRESS события.

```text
GPIO edge
   |
   v
btn_hw -- debounce 10 ms --> confirmed pressed state
   |
   v
btn -- double-press window 1000 ms --> application callback
```

Первое подтверждённое состояние после старта используется для инициализации и не создаёт события. Каждый последующий завершённый press→release цикл формирует SINGLE_PRESS. Если второй цикл завершается в течение окна 1000 мс от первого отпускания, дополнительно формируется DOUBLE_PRESS.

Общую инфраструктуру LFCLK/app_timer до инициализации кнопки подготавливает [timer_service](../timer_service/timer_service.md). `btn_hw` и `btn` создают собственные one-shot timers.
