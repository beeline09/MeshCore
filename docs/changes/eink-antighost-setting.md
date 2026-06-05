# Настройка Antighosting для eInk — отключено по умолчанию

**Файлы:** `src/helpers/NodePrefs.h`, `src/helpers/ui/GxEPDDisplay.cpp`, `src/helpers/ui/E290Display.cpp`, `src/helpers/ui/E213Display.cpp`, `examples/companion_radio/ui-new/UITask.cpp`

## Проблема

Периодический полный рефреш (full refresh) eInk-дисплея устранял артефакты
«призраков», но вызывал заметное мигание экрана каждые 20–40 обновлений.
Для многих сценариев (clock page, статичные UI) это нежелательно.

## Решение

Antighosting (full refresh) **отключён по умолчанию**. Включить его можно
через меню **Settings → Antighost: On**.

## Что изменено

### NodePrefs
Добавлено поле `uint8_t ui_eink_antighost` (0 = OFF, 1 = ON).
Дефолт 0 обеспечивается нулевой инициализацией при первом старте.

### E290Display / E213Display
Добавлен флаг `_suppress_full_refresh` и override `setFullRefreshSuppressed()`.
В `endFrame()` полный рефреш пропускается когда флаг установлен; счётчик
частичных обновлений при этом сбрасывается.

### UITask (Settings)
Новый пункт меню **Antighost** (значения `Off` / `On`) добавлен перед `Back`
во всех трёх конфигурациях сборки.

Логика передачи флага дисплею:
- **Antighost Off** (умолч.): `suppress = true` всегда → только partial refresh
- **Antighost On**: `suppress = isOnClockPage()` — поведение как прежде
  (full refresh разрешён, кроме clock-страницы)

## Поведение по умолчанию

Устройство с prefs от предыдущей прошивки получит `ui_eink_antighost = 0`
(поле добавлено в конец структуры → читается как 0). Antighosting будет
выключен до явного включения пользователем.
