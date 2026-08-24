# AGENTS.md

## Communication preferences

- Always communicate with the repository owner in **Russian**, even when they write in English. Do not switch to another language unless they explicitly ask for it in a given request.

## Design principles (hard rules)

- **Не лезть в core и не ломать существующую логику.** Это касается и `src/`, и
  `examples/` — обе папки идут из upstream, правки в них создают конфликты при синхронизации.
- **Работаем в папке своего варианта** (`variants/<board>/`). Образец — `variants/darktec/`:
  свой класс платы, свой дисплей, своя обёртка радио, всё подключается дефайнами
  (`WRAPPER_CLASS`, `DISPLAY_CLASS`) или переопределением макросов в `target.h` / `variant.h`.
  Нужное поведение получаем через существующие виртуальные хуки
  (`onBeforeTransmit()`, `onAfterTransmit()`, `recvRaw()`, `loop()`), а не правкой их вызывающих.
  Чтобы отключить чужой код, достаточно не объявлять пин в `variant.h` — блоки `#ifdef` погаснут сами.
- PlatformIO складывает все `-D` в `CPPDEFINES` и выводит `-U` **после** них, поэтому
  переопределить унаследованный дефайн через `-U` в `build_flags` нельзя — только
  `#undef` / `#define` в заголовке варианта.
- **Настройки ноды принадлежат пользователю.** Дефайн сборки может задать *начальное*
  значение при первом старте (как `ADVERT_NAME`), но не имеет права перезаписывать то,
  что уже сохранено в `NodePrefs` и настроено через приложение/CLI. Никаких
  «принудительно применить дефайн на каждом boot».

## Cursor Cloud specific instructions

This repo is the **MeshCore `south_edition` firmware** — embedded C++ built with **PlatformIO** (Arduino framework). There are **no servers, databases, or long-running services**; the product is firmware binaries flashed onto LoRa radio boards. "Running the app" without physical hardware means (a) running the native host unit tests and (b) compiling a firmware target into a `.bin`/`.uf2`.

### Toolchain / how it runs
- The build engine is the PlatformIO CLI (`pio`), installed via `pip install --upgrade platformio` (see the update script). It lands in `~/.local/bin`, which has been added to `PATH` in `~/.bashrc`. If `pio` is not found in a fresh shell, run `export PATH="$HOME/.local/bin:$PATH"`.
- First `pio test`/`pio run` for a given platform downloads large toolchains/libraries (espressif32, nordicnrf52, etc.) into `~/.platformio` and needs network access. Subsequent builds use that cache. An ESP32-S3 first build takes ~2 min; incremental builds are seconds.

### Test / build / run (no hardware needed)
- Unit tests (host/native, GoogleTest): `pio test -e native -vv`. Only `src/Utils.cpp` is covered today (5 tests in `test/test_utils/`). Note the trailing PlatformIO summary prints `0 test cases: 0 succeeded` — that is a known cosmetic quirk; trust the `native:test_utils [PASSED]` line instead.
- List all build targets (environments): `sh build.sh list` or `pio project config`. There are 800+ `<board>_<app>` env combinations from `variants/` × `examples/`.
- Build one firmware target: `pio run -e <env>` (e.g. `pio run -e Heltec_v3_companion_radio_ble`). Output binary lands in `.pio/build/<env>/firmware.bin` (ESP32) or `firmware.uf2` (nRF52/RP2040). The `.github/workflows/pr-build-check.yml` matrix lists representative envs across each platform.
- Release-style build with versioning/packaging into `out/`: `FIRMWARE_VERSION=v0.0.0 sh build.sh build-firmware <env>`.
- Flashing (`pio run -e <env> -t upload`) and `pio device monitor` require physical hardware/USB and cannot run in this VM.

### Lint / formatting
- No CI lint job. A `.clang-format` exists but is **not** enforced; per README/CONTRIBUTING, do **not** retroactively reformat existing code (it creates noisy diffs).

### PR conventions
- Per README, the intended base branch for contributions is `dev`. Automated-agent PRs may append `🤖🤖` to the PR title to opt into fast-track merging (see `CONTRIBUTING.md`).
