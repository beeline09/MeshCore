# Фиксы: TerminalCLI перетирал канал + часы на NRF52

**Файлы:** `src/helpers/BaseChatMesh.cpp`, `examples/companion_radio/ui-new/UITask.cpp`

---

## 1. TerminalCLI перетирал последний пользовательский канал

### Симптом

После обновления прошивки (или при первом появлении `WITH_COMPANION_CLI`) канал,
добавленный пользователем, исчезал и заменялся каналом `TerminalCLI`.

### Причина

`addChannel()` использует `num_channels` как индекс следующей записи.
`loadChannels()` восстанавливает каналы через `setChannel(idx, ch)` — прямая запись
в массив по индексу, **`num_channels` при этом не обновлялась**.

Последовательность при старте:

1. `addChannel("Public", …)` → `channels[0]`, `num_channels = 1`
2. `loadChannels()` → `setChannel(0, Public)`, `setChannel(1, MyChan)` — `num_channels` остаётся `1`
3. Скан 0..MAX — `TerminalCLI` не найден → `addChannel("TerminalCLI")` → пишет в `channels[1]` ← **затирает MyChan**

Чаще всего проявлялось при обновлении с прошивки без `TerminalCLI` на прошивку с ним.

### Фикс

`src/helpers/BaseChatMesh.cpp`, функция `setChannel()`:

```cpp
if (idx >= num_channels) num_channels = idx + 1;
```

После `loadChannels()` `num_channels` отражает реальное количество занятых слотов,
и `addChannel("TerminalCLI")` записывается следующим, не затирая загруженные каналы.

Механизм проверки и авто-регистрации `TerminalCLI` при отсутствии уже существовал
(MyMesh.cpp:1043–1057) и продолжает работать корректно.

---

## 2. Часы не работали на NRF52 платах

### Симптом

CLOCK-страница на всех NRF52 companion-нодах (ProMicro, GAT562 и др.) показывала
неизменное время независимо от настроек синхронизации.

### Причина

`examples/companion_radio/ui-new/UITask.cpp`, CLOCK page:

```cpp
time_t now = time(nullptr);   // POSIX time
```

На **ESP32** `setCurrentTime()` вызывает `settimeofday()` — системное POSIX-время
обновляется, `time()` возвращает правильное значение.

На **NRF52** нет hardware RTC на ProMicro/GAT562 → используется `VolatileRTCClock`.
Его `setCurrentTime()` обновляет внутренний `base_time`, но **не вызывает `settimeofday()`**.
`time(nullptr)` возвращает 0 вне зависимости от сетевой синхронизации.

### Фикс

```cpp
// было:
time_t now = time(nullptr);

// стало:
time_t now = (time_t)_rtc->getCurrentTime();
```

`_rtc` (`VolatileRTCClock`) корректно обновляется при сетевой синхронизации через
`TimeSyncHelper`. `localtime_r` и `strftime` не изменились.

### Затронутые платформы

Все NRF52 companion-варианты: ProMicro (все модификации), RAK 4631, RAK 3401,
LilyGo T-Echo / T-Echo Lite, GAT562 (все варианты), ikoka (handheld / nano / stick),
KeepteenLT1, Mesh Pocket, Meshtiny, Minewsemi, Nano G2 Ultra, R1Neo, SenseCap Solar,
Heltec t096 / t114, ThinkNode M1 / M3 / M6, WioTrackerL1 / Eink, wio_wm1110, Xiao nRF52.

Также RP2040 и STM32 companion-варианты (PicoW, RAK 11310, waveshare, wio-e5).

На ESP32 часы работали корректно до фикса — изменение не влияет на их поведение.
