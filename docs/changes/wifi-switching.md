# WiFi switching — динамическое переключение BLE / WiFi / USB

## Проблема

Транспорт companion radio выбирался на этапе компиляции: отдельные прошивки
`_ble`, `_wifi`, `_usb`. Сменить режим без перепрошивки было невозможно.

## Решение

Новый флаг `WITH_WIFI_SWITCHING` активирует runtime-переключение транспорта.
Все три стека (BLE, WiFi TCP, USB CDC) включены в одну прошивку; активный
интерфейс меняется через меню Settings или CLI без перезагрузки.

Режим сохраняется в `/wifi_prefs` (SPIFFS) и восстанавливается после ребута.

## Новые env — `*_companion_radio_uni`

Добавлены во все ESP32/ESP32-S3 платформы с поддержкой WiFi:

| Платформа | Env |
|-----------|-----|
| Heltec E213 | `Heltec_E213_companion_radio_uni`, `Heltec_E213_screen_180_companion_radio_uni` |
| Heltec E290 | `Heltec_E290_companion_uni`, `Heltec_E290_screen_180_companion_uni` |
| Heltec Wireless Paper | `Heltec_Wireless_Paper_companion_radio_uni`, `Heltec_Wireless_Paper_screen_180_companion_radio_uni` |
| Heltec V2 | `Heltec_v2_companion_radio_uni` |
| Heltec V3 / WSL3 | `Heltec_v3_companion_radio_uni`, `Heltec_WSL3_companion_radio_uni` |
| Heltec V4 | `heltec_v4_companion_radio_uni`, `heltec_v4_tft_companion_radio_uni` |
| Heltec Tracker V2 | `heltec_tracker_v2_companion_radio_uni` |
| LilyGo TBeam 1W | `LilyGo_TBeam_1W_companion_radio_uni` |
| LilyGo TBeam S3 Supreme | `T_Beam_S3_Supreme_SX1262_companion_radio_uni` |
| LilyGo TLora V2.1 | `LilyGo_TLora_V2_1_1_6_companion_radio_uni` |
| Nibble Screen Connect | `nibble_screen_connect_companion_radio_uni` |
| RAK3112 | `RAK_3112_companion_radio_uni` |
| Station G2 | `Station_G2_companion_radio_uni` |
| ThinkNode M2 | `ThinkNode_M2_companion_radio_uni` |
| ThinkNode M5 | `ThinkNode_M5_companion_radio_uni` |
| Xiao C3 | `Xiao_C3_companion_radio_uni` |
| Xiao S3 WIO | `Xiao_S3_WIO_companion_radio_uni` |

## Settings → Comms

Новый пункт `Comms` в странице Settings (только при `WITH_WIFI_SWITCHING`).

Отображает текущий режим рядом с меткой:
```
> Comms   BLE
> Comms   WiFi ...    (подключение)
> Comms   WiFi OK     (подключён)
> Comms   USB
```

На главном экране (FIRST):
- вместо BLE PIN — `IP:192.168.x.x` когда WiFi подключён
- жёлтый `WiFi...` во время подключения
- PIN показывается только в BLE-режиме

### Сабскрин выбора сети

При нажатии на Comms запускается асинхронное сканирование WiFi (~2–4 с).

```
  Scanning WiFi...

после сканирования:

>*HomeNet        -65   ← сохранена, видна в эфире
 *OfficeWifi     ---   ← сохранена, не в эфире
  GuestWifi      -72   ← в эфире, не сохранена
  OpenNet        -88
  BLE
  USB
  Back
```

- `*` — сеть сохранена (есть пароль)
- RSSI или `---` если вне зоны видимости
- Нажать Enter на сохранённой (`*`) → подключение
- Нажать Enter на несохранённой → полноэкранный оверлей с командой:
  ```
  wifi add
  GuestWifi
  <pass>
  ```
  Любая кнопка закрывает оверлей, возвращает в список
- BLE / USB → мгновенное переключение

## CLI команды (только `WITH_WIFI_SWITCHING`)

| Команда | Действие |
|---------|---------|
| `wifi list` | список сохранённых сетей с индексами |
| `wifi scan` | сканирование эфира (блокирующий, ~2 с) |
| `wifi add <ssid> <pass>` | добавить/обновить сеть (макс. 5) |
| `wifi del <ssid>` | удалить сеть |
| `wifi connect <idx>` | подключиться к сети по индексу из `wifi list` |
| `wifi status` | текущий режим, IP, порт |
| `wifi ip dhcp` | режим DHCP |
| `wifi ip static <ip> <gw> <mask>` | статический IP |
| `wifi port <1-65535>` | TCP порт (по умолчанию 5000) |

## Поведение при переключении

- **→ WiFi**: BLE остаётся активным как fallback на время подключения (до 15 с).
  После успешного подключения BLE отключается, стартует TCP-сервер.
  Таймаут → откат в BLE, режим в prefs меняется на BLE.
- **→ BLE**: WiFi отключается, BLE-реклама стартует.
  PIN генерируется случайно при первом запуске (6 цифр), сохраняется в prefs.
- **→ USB**: оба стека отключаются, интерфейс — `Serial` (CDC).

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `examples/companion_radio/WifiPrefs.h` | новый файл: структуры `WifiPrefs`, `WifiNetwork`, константы режимов |
| `examples/companion_radio/NodePrefs.h` | поле `comms_mode` в конце struct |
| `examples/companion_radio/MyMesh.h` | члены `_ble_iface`, `_wifi_iface`, `_usb_iface`, `_wifi_prefs`; методы управления |
| `examples/companion_radio/MyMesh.cpp` | `switchCommsMode()`, `checkWifiConnection()`, `initCommsFromPrefs()`, CLI wifi-команды |
| `examples/companion_radio/main.cpp` | ветка `WITH_WIFI_SWITCHING` в `setup()` |
| `examples/companion_radio/ui-new/UITask.cpp` | пункт Comms в Settings, сабскрин сканирования, оверлей команды, IP на главном экране |
| `variants/*/platformio.ini` × 17 | новые env `*_companion_radio_uni` |
| `build-local.ps1` | fallback определения платформы при недоступном `pio --json-output` |
