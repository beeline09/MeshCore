# Companion Radio — CLI управление

## Проблема

Companion radio узлы не имели интерфейса для изменения параметров в runtime без перепрошивки.
Ретрансляторы давно имеют `CommonCLI`, но companion не мог его использовать из-за конфликтов
структур и отсутствия подходящего канала доставки команд.

## Решение

Два независимых режима CLI, реализованных в `examples/companion_radio/MyMesh.cpp`:

### Approach 1 — Remote CLI (PM + PIN)

Пользователь отправляет личное сообщение (PM) в формате `<PIN> <команда>`.

- **PIN** — 8-символьный hex от последних 4 байт **приватного** ключа.
  Выводится в Serial при загрузке: `CLI PIN: A1B2C3D4`.
  Запрашивается через TerminalCLI командой `pin`.
- Перехват в `onMessageRecv()`: если текст начинается с совпадающего PIN + пробел —
  вызывается `handleRemoteCLI()`, обычная маршрутизация не происходит.
- Ответ отправляется PM-ом обратно отправителю.
- Включён/выключен флагом `_remote_cli_enabled` (runtime, `set remote.cli on/off`).

**Безопасность PIN**: PIN не выводим из публичного ключа (который виден всем узлам сети
через advert-пакеты). Используются байты приватного ключа, известные только владельцу ноды.

### Approach 2 — TerminalCLI (канал)

Специальный канал `TerminalCLI` регистрируется автоматически при загрузке прошивки
(с фиксированным PSK). Сообщения в этом канале перехватываются в `handleCmdFrame()` —
они **не** идут в LoRa, обрабатываются локально, ответ возвращается в канал.

- Доступен только через BLE/WiFi/USB подключение к ноде.
- Команды вводятся без PIN.
- Включён/выключен флагом `_terminal_cli_enabled` (runtime, `set terminal.cli on/off`).
- `set terminal.cli off` удаляет канал из списка и сохраняет изменение.

## Длинные ответы

Ответный буфер — 512 байт. Если ответ длиннее 150 байт, он разбивается на чанки
с маркерами `[1/N]`, `[2/N]` и т.д.

## Реализованные команды

Команды обрабатываются в двух уровнях:

1. **`handleCliCmd()`** — команды, специфичные для companion radio
2. **`CommonCLI::handleCommand()`** — общие команды (радио, конфигурация, GPS и т.д.)

### Companion-специфичные команды

| Команда | Описание |
|---------|----------|
| `reboot` | Ответ перед перезагрузкой (через 1 с) |
| `poweroff` / `shutdown` | Ответ перед выключением (через 0.5 с) |
| `pin` | Показать текущий CLI PIN |
| `get remote.cli` / `set remote.cli on/off` | Управление Remote CLI |
| `get terminal.cli` / `set terminal.cli on/off` | Управление TerminalCLI |
| `timesync` | Подробный статус синхронизации |
| `timesync on/off` | Включить/выключить всю автосинхронизацию |
| `timesync adverts on/off` | Синхронизация по advert-таймстемпам |
| `timesync msgs on/off` | Синхронизация по таймстемпам сообщений |
| `timesync reset` | Сбросить буфер и счётчики |
| `timesync params` | Показать runtime-параметры кворума |
| `set timesync.cluster <сек>` | Окно кластеризации (по умолч. 60 с) |
| `set timesync.drift <сек>` | Порог дрейфа для коррекции (по умолч. 120 с) |
| `set timesync.jump <сек>` | Максимальный прыжок времени (по умолч. 36000 с) |

Параметры `timesync.*` — **runtime only**, сбрасываются при перезагрузке.
Флаги `remote.cli` и `terminal.cli` — **runtime only**, восстанавливаются в `on` при перезагрузке.

## Исправления

**Trailing whitespace → "Unknown command"**  
Если companion-app добавляет `\r` или `\n` в конце текста PM, команда приходила как
`"timesync\r"`. `strcmp` не совпадал ни в `handleCliCmd`, ни в CommonCLI (`command[8]` не был
`\0` или `' '`), и оба пути заканчивались "Unknown command".  
Фикс: `handleRemoteCLI` и `handleTerminalCLI` обрезают trailing whitespace перед любым сравнением.

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `examples/companion_radio/MyMesh.h` | CLI члены, флаги runtime, объявление `handleCliCmd` |
| `examples/companion_radio/MyMesh.cpp` | Инициализация, перехват PM и TerminalCLI, `handleCliCmd` |
| `examples/companion_radio/CompanionCLICallbacks.h` | Интерфейс для `CommonCLI` |
| `examples/companion_radio/CompanionCLICallbacks.cpp` | Реализация колбэков |
| `src/helpers/TimeSyncHelper.h` | Runtime-параметры кворума, метод `reset()` |
| `src/helpers/TimeSyncHelper.cpp` | Использование runtime-параметров вместо констант |
