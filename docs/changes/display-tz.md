# DISPLAY_TZ — часовой пояс для страницы CLOCK

## Проблема

Страница CLOCK выводила время в UTC, даже если пользователь находился в другом часовом поясе.
Флаг `-DDISPLAY_TZ` был доступен только в вариантах e-ink (heltec_e213, heltec_e290),
остальные варианты с дисплеем его не имели.

## Решение

### Применение часового пояса

В `UITask::begin()` добавлена инициализация POSIX-переменной окружения:

```cpp
#ifdef DISPLAY_TZ
  setenv("TZ", DISPLAY_TZ, 1);
  tzset();
#endif
```

После этого `localtime_r()` автоматически конвертирует UTC → локальное время при рендере страницы CLOCK.

### Значение по умолчанию во всех вариантах с дисплеем

В каждый `target.h` вариантов с дисплеем добавлен блок:

```cpp
#ifndef DISPLAY_TZ
#  define DISPLAY_TZ  "UTC0"
#endif
```

Пользователь может переопределить в `platformio.ini`:
```ini
-DDISPLAY_TZ="MSK-3"
```

### Синтаксис значения

Строка в формате POSIX TZ (RFC 6838 / GNU libc):

| Строка | Часовой пояс |
|--------|-------------|
| `UTC0` | UTC (по умолчанию) |
| `MSK-3` | Москва UTC+3 |
| `EET-2` | Восточная Европа UTC+2 |
| `CET-1CEST,M3.5.0,M10.5.0/3` | Центральная Европа с переходом на летнее время |

Подробнее: [GNU libc TZ Variable](https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html)

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `variants/*/target.h` (все варианты с дисплеем) | `#ifndef DISPLAY_TZ` блок с дефолтом `UTC0` |
| `variants/heltec_e213/platformio.ini` | Упрощение синтаксиса: `-DDISPLAY_TZ="MSK-3"` |
| `variants/heltec_e290/platformio.ini` | Упрощение синтаксиса: `-DDISPLAY_TZ="MSK-3"` |
| `examples/companion_radio/ui-new/UITask.cpp` | `setenv("TZ", DISPLAY_TZ, 1)` + `tzset()` в `begin()` |
