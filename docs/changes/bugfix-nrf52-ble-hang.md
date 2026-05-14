# Фиксы: зависание NRF52 на BLE-команде и переполнение стека remote CLI

**Файлы:** `examples/companion_radio/MyMesh.cpp`, `examples/companion_radio/MyMesh.h`,
`src/helpers/BaseChatMesh.cpp`

---

## 1. Переполнение стека loop_task при remote CLI команде

### Симптом

Нода на NRF52840 (Heltec T114, ProMicro и другие BLE-варианты) зависала намертво сразу
после получения сообщения с CLI-пином — например `607BAA84 timesync`. Сброс только вручную.
В Serial-логе пусто: зависание происходило внутри обработчика, до любого `println`.

### Причина

FreeRTOS `loop_task` имеет стек 4096 байт (`LOOP_STACK_SZ = 256 × sizeof(uint32_t)`).
Цепочка вызовов при обработке remote CLI-команды:

```
BaseChatMesh::loop()
  → onMessageRecv()
    → handleRemoteCLI()        // char buf[512] + char cmdBuf[256]  = 768 B локалей
      → sendCliReplyPM()       // char chunks[8][152] + char text[160] = 1376 B локалей
        → sendMessage()        // дополнительный фрейм на стеке
```

Суммарно только два промежуточных фрейма занимали 768 + 1376 = **2144 B** плюс
накопленный overhead вышестоящих вызовов — итого более 4096 B → немедленный hard fault
без какого-либо вывода в лог.

Аналогичная ситуация в `handleTerminalCLI` → `sendCliReplyChannel`.

### Фикс

`examples/companion_radio/MyMesh.cpp` — большие локальные массивы во всех четырёх
функциях переведены в `static`:

```cpp
// handleRemoteCLI — экономит 768 B со стека
static char cmdBuf[256];
static char buf[512];

// handleTerminalCLI — то же самое
static char cmdBuf[256];
static char buf[512];

// sendCliReplyPM — экономит 1376 B со стека
static char chunks[8][152];
static char text[160];

// sendCliReplyChannel
static char chunks[8][152];
static char text[200];
```

`static` безопасен: `loop_task` однопоточен, эти функции никогда не вызываются
реентерабельно. Буферы выделяются в BSS один раз при старте и не занимают стек.

---

## 2. Дедлок flash при записи настроек во время BLE-соединения

### Симптом

Нода зависала при сохранении настроек или контактов в момент активного BLE-соединения.
Характерно для NRF52840 с SoftDevice S140 v6.1.1. Лог при этом мог обрываться на
`[FLASH]`-строке, либо зависание происходило в `savePrefs()` / `saveContacts()`.

### Причина

`flash_nrf5x.c` (Adafruit nRF52 BSP) при стирании/записи флэша вызывает
`xSemaphoreTake(_sem, portMAX_DELAY)`. S140 v6.1.1 отдаёт управление только
в «радионейтральные» окна (radio idle windows). Пока BLE-соединение активно,
таких окон может не быть — семафор блокирует `loop_task` **навсегда**.

До этого фикса `savePrefs()` и `saveContacts()` вызывались напрямую из `loop()` по
истечении таймера отложенной записи, без проверки состояния BLE.

### Фикс

`examples/companion_radio/MyMesh.cpp`, `loop()` — добавлена проверка `isConnected()`
с повтором через 1 секунду:

```cpp
bool ble_busy = _serial && _serial->isConnected();
if (dirty_contacts_expiry && millisHasNowPassed(dirty_contacts_expiry)) {
    if (!ble_busy) {
        saveContacts();
        dirty_contacts_expiry = 0;
    } else {
        dirty_contacts_expiry = futureMillis(1000);
    }
}
if (dirty_prefs_expiry && millisHasNowPassed(dirty_prefs_expiry)) {
    if (!ble_busy) {
        savePrefs();
        dirty_prefs_expiry = 0;
    } else {
        dirty_prefs_expiry = futureMillis(1000);
    }
}
```

Та же проверка добавлена перед `board.reboot()` и `board.powerOff()`:
если BLE подключён — отложить, после отключения loop() запишет dirty-данные
и перезагрузит/выключит устройство.

`saveContacts()` также защищена во всех остальных местах `loop()`, где ранее
вызывалась без проверки (флаг `dirty_contacts_expiry` вместо прямого вызова).

### Затронутые платформы

Все NRF52840 companion-варианты с BLE: Heltec T114, ProMicro, RAK 4631, LilyGo T-Echo,
GAT562, Mesh Pocket, ikoka и прочие нодовые платы на nRF52840 + SoftDevice S140.

---

## 3. Прямой вызов savePrefs() из сеттеров Cyr2Lat

### Симптом

При включении/отключении функции Cyr2Lat через меню на NRF52 BLE-ноде устройство
зависало — та же картина, что и в баге #2.

### Причина

`setCyr2LatChannelsEnabled()` и `setCyr2LatContactsEnabled()` были объявлены
inline в `MyMesh.h` и вызывали `savePrefs()` напрямую:

```cpp
void setCyr2LatChannelsEnabled(bool enabled) {
    _cyr2lat_channels_enabled = enabled;
    _prefs.cyr2lat_channels = enabled ? 1 : 0;
    savePrefs();   // прямой вызов — дедлок на NRF52 при BLE
}
```

Эти сеттеры вызываются из обработчика меню UITask во время активного BLE-соединения.

### Фикс

Методы перенесены из `MyMesh.h` в `MyMesh.cpp` с заменой прямого `savePrefs()`
на отложенную запись:

```cpp
void MyMesh::setCyr2LatChannelsEnabled(bool enabled) {
    _cyr2lat_channels_enabled = enabled;
    _prefs.cyr2lat_channels = enabled ? 1 : 0;
    dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
}
void MyMesh::setCyr2LatContactsEnabled(bool enabled) {
    _cyr2lat_contacts_enabled = enabled;
    _prefs.cyr2lat_contacts = enabled ? 1 : 0;
    dirty_prefs_expiry = futureMillis(LAZY_PREFS_WRITE_DELAY);
}
```

---

## Защитный фикс: выход за границу буфера в onGroupDataRecv

**Файл:** `src/helpers/BaseChatMesh.cpp`

В функции `onGroupDataRecv()` отсутствовала проверка длины перед записью
нулевого терминатора в конец пришедших данных. При длине пакета равной
`MAX_PACKET_PAYLOAD` запись `data[len] = 0` выходила за границу массива
на стеке, потенциально повреждая соседние переменные.

```cpp
// было (без проверки):
data[len] = 0;

// стало:
if (len >= MAX_PACKET_PAYLOAD) return;   // malformed: would overflow data[] on stack
data[len] = 0;
```

Аналогичная проверка ранее уже существовала в `onPeerDataRecv()`.
