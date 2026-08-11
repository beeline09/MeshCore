# Darktec on-demand builds

Экспериментальный flasher: https://beeline09.github.io/darktec_new/

## Зачем

Каталог `darktec-latest` покрывает роль × химия × защита с дефолтными `ADVERT_NAME`.
Для кастомного имени ноды нужна отдельная сборка; полный матрикс × имена не храним.

## Как работает

1. UI считает slug имени, параметры радио и `sha8` tip `south_edition`.
2. Ищет ассеты в Release tag **`darktec-ondemand`**
   (`…__{name}__f{freq}-bw{bw}-sf…-tx…__{sha}.{uf2,zip}`).
3. При промахе — issue с блоком `<!-- darktec-ondemand ... -->` (метка `darktec-ondemand`).
4. Workflow `.github/workflows/build-darktec-ondemand.yml` + `scripts/build-darktec-ondemand.sh`:
   - один `pio` env;
   - `-DADVERT_NAME=...` и `-DDARKTEC_RADIO_CUSTOM -DLORA_*=...`;
   - **не** вызывает `darktec-version.sh` (не бампит `bN`);
   - заливает ассеты с `--clobber`.

Триггеры: `workflow_dispatch`, `repository_dispatch` (`darktec-ondemand`), issues (метка / marker / title `darktec-ondemand:`).

## Ограничения

- Serial DFU с GitHub asset URL упирается в CORS — в lab пока UF2-скачивание; каталогные zip с same-origin/`darktec-latest` как раньше.
- Публичный one-click без issue потребует PAT/Cloudflare Worker.
