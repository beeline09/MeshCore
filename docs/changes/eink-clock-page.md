# UITask — страница часов для eInk-дисплеев

**Файлы:** `examples/companion_radio/ui-new/UITask.cpp`, `examples/companion_radio/ui-new/UITask.h`,
`src/helpers/ui/DisplayDriver.h`, `src/helpers/ui/E213Display.h`,
`src/helpers/ui/E290Display.h`, `src/helpers/ui/GxEPDDisplay.h`

**Ветка:** `feature/eink-clock-page` (от `Cyrillic_supp4all`)

## Вектор изменений

Добавлена отдельная страница `CLOCK` в HomeScreen для устройств с eInk-дисплеем. Страница отображает крупные часы и умеет показывать входящие личные сообщения (PM) прямо на себе, не переключая экран. Канальные сообщения при нахождении на странице часов ставятся в очередь без переключения на MsgPreviewScreen.

---

## 1. Runtime-определение eInk через виртуальный метод

### `src/helpers/ui/DisplayDriver.h`

```cpp
virtual bool isEink() const { return false; }  // + добавлен
```

### eInk-драйверы: `E213Display.h`, `E290Display.h`, `GxEPDDisplay.h`

```cpp
bool isEink() const override { return true; }  // + добавлен в каждый
```

Подход без компайл-флагов: UITask спрашивает `_display->isEink()` в runtime. Это позволяет использовать один и тот же скомпилированный код для eInk и не-eInk вариантов.

---

## 2. Новая страница CLOCK в HomeScreen

### Место в навигации

```
FIRST → CLOCK → RECENT → RADIO → BLUETOOTH → ADVERT → [GPS] → [SENSORS] → SHUTDOWN
```

Страница `CLOCK` доступна на всех устройствах с дисплеем (OLED и e-ink).
На e-ink используется шрифт `setTextSize(8)` (крупный), на OLED — `setTextSize(3)`.

### Enum `HomePage`

```cpp
enum HomePage {
  FIRST,
  CLOCK,     // large clock + PM preview inline  ← новый
  RECENT,
  ...
};
```

### Индикатор страниц (точки)

```cpp
int visible_count = HomePage::Count;  // все страницы видимы
```

---

## 3. Рендер страницы CLOCK

Страница имеет два состояния в зависимости от наличия непрочитанных PM.

### Состояние «часы» (`_clock_pm_pending == 0`)

На e-ink:
```
     14:35
  Mon 04 May
   advert +2s
```

На OLED (шрифт меньше, экономит место):
```
  14:35
Mon 04 May
 advert +2s
```

Рендер: часы → дата (`%a %d %b`) → источник синхронизации (`advert +2s` / `GPS` / ...).

```cpp
// e-ink
display.setTextSize(8);
display.drawTextCentered(..., "HH:MM");
display.drawTextCentered(display.width()/2, display.height()-20, dateBuf);  // дата
display.drawTextCentered(display.width()/2, display.height()-10, sourceBuf);
return 60000;  // обновление раз в минуту

// OLED
display.setTextSize(3);
display.drawTextCentered(..., "HH:MM");
display.drawTextCentered(..., dateBuf);
display.drawTextCentered(..., sourceBuf);
```

### Состояние «PM» (`_clock_pm_pending > 0`)

Полная страница в стиле MsgPreviewScreen:
- Строка заголовка: `#N (D)имя` + разделитель
- Тело сообщения через `printWordWrap()`
- Кнопка KEY_NEXT — следующий PM

```cpp
return 60000;  // не авто-обновляться, ждать ввода
```

---

## 4. Навигация по PM (handleInput)

Перехват до стандартной навигации:

```cpp
if (_page == CLOCK && _clock_pm_pending > 0) {
  KEY_NEXT  → consumeTopMsg() + _clock_pm_pending--   // следующий PM / возврат к часам
  KEY_PREV  → поглотить (блокировать уход со страницы)
}
```

Когда `_clock_pm_pending` достигает нуля, `render()` автоматически переключается в режим часов.

---

## 5. Маршрутизация сообщений (UITask::newMsg)

| Условие | Поведение |
|---------|-----------|
| Устройство не eInk | Стандарт: addPreview + setCurrScreen(msg_preview) |
| На CLOCK, пришёл **PM** (path_len == 0xFF) | addPreview + incrementClockPM + _next_refresh=100; **переключения нет** |
| На CLOCK, пришло **канальное** сообщение | Только addPreview в MsgPreviewScreen; **переключения нет** |
| Не на CLOCK | Стандарт |

PM попадает в обе очереди: `MsgPreviewScreen` (для последующего просмотра вручную) и счётчик `_clock_pm_pending` HomeScreen (для inline-отображения). Прочтение на странице часов через `consumeTopMsg()` синхронно уменьшает `num_unread` в MsgPreviewScreen.

---

## 6. Новые публичные API

### `UITask.h`

```cpp
bool isEinkDisplay() const;

struct ClockPMInfo {
  uint8_t path_len;
  char    from_name[32];
  char    msg[10 * CIPHER_BLOCK_SIZE];
};
bool peekTopMsg(ClockPMInfo& out) const;   // читает без потребления
void consumeTopMsg();                        // помечает как прочитанное
int  getUnreadMsgCount() const;
```

### HomeScreen (внутри UITask.cpp)

```cpp
bool isOnClockPage() const;    // используется в UITask::newMsg()
void incrementClockPM();       // вызывается при входящем PM на CLOCK
```

### MsgPreviewScreen (внутри UITask.cpp)

```cpp
const MsgEntry* peekCurrent() const;   // текущее непрочитанное без потребления
void consumeOne();                       // advance без переключения экрана
int  unreadCount() const;
```

`MsgEntry` struct переведена в `public` секцию для доступа из `UITask::peekTopMsg()`.
