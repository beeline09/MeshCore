# Сборка и GitHub Actions — руководство

## build-local.ps1 — локальная сборка

Скрипт для локальной сборки одного или нескольких PlatformIO env на Windows.
Собирает прошивку, генерирует merged.bin для ESP32 и uf2/zip для NRF52,
складывает артефакты в `out/` с именами по конвенции GHA.

```powershell
.\build-local.ps1 Heltec_v3_companion_radio_uni
.\build-local.ps1 Heltec_v3_companion_radio_uni, Heltec_E213_companion_radio_uni
.\build-local.ps1 -Envs Heltec_v3_companion_radio_uni -DisableDebug
```

**Версия** берётся из последнего git-тега (`git describe --tags --abbrev=0`),
короткий хеш коммита добавляется суффиксом: `v1.15.0b9-abcdef12`.

**Платформа** определяется через `pio project config --json-output` (парсинг build_flags
на наличие `ESP32_PLATFORM` / `NRF52_PLATFORM` / `STM32_PLATFORM` / `RP2040_PLATFORM`).
При недоступности pio json — ошибка, платформа не определена (`unknown`), сборка продолжается
но mergebin/uf2 не запускаются.

**`-DisableDebug`** — убирает флаги отладки (`MESH_DEBUG`, `BLE_DEBUG_LOGGING` и др.)
для production-сборки.

---

## build.sh — CI-сборка

Основной скрипт сборки для GitHub Actions (Linux/Ubuntu).

### Функции

| Вызов | Описание |
|-------|----------|
| `build.sh build-firmware <env> [env…]` | Собрать конкретные env |
| `build.sh build-companion-firmwares` | Все `*_companion_radio_uni` |
| `build.sh build-repeater-firmwares` | Все `*_repeater` |
| `build.sh build-room-server-firmwares` | Все `*_room_server` |
| `build.sh build-matching-firmwares <spec>` | Все env содержащие `<spec>` |
| `build.sh list` | Список всех env из pio config |

### Companion-сборки

```bash
build_companion_firmwares() {
  build_all_firmwares_by_suffix "_companion_radio_uni"
}
```

Все companion-варианты унифицированы в суффикс `_companion_radio_uni`.
Старые `_ble` / `_usb` / `_wifi` суффиксы не удалены из ini-файлов (для ручных сборок),
но не собираются в CI.

---

## GitHub Actions — build-companion-firmwares.yml

### Триггеры

| Триггер | Поведение |
|---------|-----------|
| `push: tags: companion-*` | Полная сборка всех `_companion_radio_uni`, создаёт черновик релиза |
| `workflow_dispatch` (без параметров) | То же что и при теге, без релиза |
| `workflow_dispatch` + `envs` | Сборка только указанных env |

### Inputs для ручного запуска

| Параметр | Описание | Пример |
|----------|----------|--------|
| `envs` | Env через пробел (пусто = все `_companion_radio_uni`) | `ProMicro_companion_radio_ble RAK_4631_companion_radio_ble` |
| `version_tag` | Тег для версии прошивки (пусто = последний `companion-*` тег) | `companion-v1.15.0b9` |
| `release_tag` | Добавить артефакты в существующий релиз (пусто = не загружать) | `companion-v1.15.0b9` |

### Версия прошивки

- При пуше тега: извлекается из `GITHUB_REF` (`companion-v1.15.0b9` → `v1.15.0b9`)
- При `workflow_dispatch`: из `version_tag` input; если пусто — `git describe --tags --abbrev=0 --match 'companion-*'`

Версия передаётся в `build.sh` через `FIRMWARE_VERSION`, инжектируется как
`-DFIRMWARE_VERSION="v1.15.0b9-abcdef12"`.

### Релиз

- Создаётся только при пуше тега — черновик (`draft: true`) с телом из аннотации тега
- При `workflow_dispatch` с `release_tag`: файлы **добавляются** в существующий релиз
  через `gh release upload --clobber` (файлы с тем же именем перезаписываются,
  остальные ассеты релиза не трогаются)

### Типичные сценарии

**Выпуск новой версии:**
```
git tag -a companion-v1.15.0b10 -m "## What's new…"
git push origin companion-v1.15.0b10
```
→ CI соберёт все `_companion_radio_uni`, создаст черновик релиза.

**Добилдить NRF52 BLE варианты и добавить в существующий релиз:**
```
Actions → Build Companion Firmwares → Run workflow
  envs:        ProMicro_companion_radio_ble RAK_4631_companion_radio_ble …
  version_tag: companion-v1.15.0b9
  release_tag: companion-v1.15.0b9
```

**Тестовая сборка без релиза:**
```
Actions → Build Companion Firmwares → Run workflow
  envs:        Heltec_v3_companion_radio_uni
  version_tag: (пусто)
  release_tag: (пусто)
```
→ Артефакты доступны как workflow artifact 90 дней.
