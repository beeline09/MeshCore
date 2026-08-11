#!/usr/bin/env bash
# Build all Darktec roles × battery chemistry × battery protect into out/.
# Filenames: Darktec_{role}_{chem}_{cells}s_{adc|off}.{uf2,zip}
#
# Optional filters (CI shards / local subset):
#   DARKTEC_ROLE_SLUG=companion_radio_ble
#   DARKTEC_PIO_ENV=Darktec_companion_radio_ble
# When set, only matching role(s) are built (chem×protect still full).
# When unset, builds the full 8×4×2 = 64 matrix.
#
# NOTE: do NOT call build.sh per variant — it runs `rm -rf out` on every invoke.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if [ -z "${FIRMWARE_VERSION:-}" ]; then
  echo "FIRMWARE_VERSION must be set"
  exit 1
fi

COMMIT_HASH="$(git rev-parse --short HEAD)"
FIRMWARE_VERSION_STRING="${FIRMWARE_VERSION}-${COMMIT_HASH}"

rm -rf out
mkdir -p out

VARIANTS=(
  "BATTERY_CHEM_LIION liion 1"
  "BATTERY_CHEM_LIFEPO4 lifepo4 1"
  "BATTERY_CHEM_LTO lto 1"
  "BATTERY_CHEM_LTO lto 2"
)

# macro slug
PROTECTS=(
  "DARKTEC_BATT_PROTECT_ADC adc"
  "DARKTEC_BATT_PROTECT_OFF off"
)

# pio_env role_slug
ROLES=(
  "Darktec_companion_radio_ble companion_radio_ble"
  "Darktec_companion_radio_usb companion_radio_usb"
  "Darktec_repeater repeater"
  "Darktec_repeater_bridge_rs232_serial1 repeater_bridge_rs232"
  "Darktec_room_server room_server"
  "Darktec_terminal_chat terminal_chat"
  "Darktec_sensor sensor"
  "Darktec_kiss_modem kiss_modem"
)

if [ -n "${DARKTEC_PIO_ENV:-}${DARKTEC_ROLE_SLUG:-}" ]; then
  FILTERED=()
  for role in "${ROLES[@]}"; do
    read -r pio_env role_slug <<<"$role"
    if [ -n "${DARKTEC_PIO_ENV:-}" ] && [ "$pio_env" != "$DARKTEC_PIO_ENV" ]; then
      continue
    fi
    if [ -n "${DARKTEC_ROLE_SLUG:-}" ] && [ "$role_slug" != "$DARKTEC_ROLE_SLUG" ]; then
      continue
    fi
    FILTERED+=("$role")
  done
  if [ "${#FILTERED[@]}" -eq 0 ]; then
    echo "ERROR: no roles matched filter DARKTEC_PIO_ENV=${DARKTEC_PIO_ENV:-} DARKTEC_ROLE_SLUG=${DARKTEC_ROLE_SLUG:-}"
    exit 1
  fi
  ROLES=("${FILTERED[@]}")
  echo "Role filter active: ${#ROLES[@]} role(s) → ${ROLES[*]}"
fi

EXPECTED=$(( ${#ROLES[@]} * ${#VARIANTS[@]} * ${#PROTECTS[@]} ))

build_one() {
  local pio_env="$1"
  local role_slug="$2"
  local chem_macro="$3"
  local chem_slug="$4"
  local cells="$5"
  local protect_macro="$6"
  local protect_slug="$7"
  local out_base="Darktec_${role_slug}_${chem_slug}_${cells}s_${protect_slug}"
  local out_name="${out_base}.uf2"
  local versioned_name="${pio_env}-${FIRMWARE_VERSION_STRING}-${protect_slug}.uf2"

  echo "=== Building ${out_name} (${pio_env}, ${chem_macro}, cells=${cells}, protect=${protect_macro}) ==="

  # Do NOT use -UBATTERY… here: PlatformIO moves all -U* to the end of gcc argv,
  # so -UBATTERY -DBATTERY=… leaves the macro UNDEFINED (chemistry falls back to
  # #ifndef defaults). Later -D overrides ini; build_quiet.h #undef's debug flags.
  QUIET_H="${ROOT}/variants/darktec/build_quiet.h"
  export PLATFORMIO_BUILD_FLAGS="-DBATTERY_CHEMISTRY=${chem_macro} -DBATTERY_CELLS=${cells} -DDARKTEC_BATT_PROTECT=${protect_macro} -DCFG_DEBUG=0 -DFIRMWARE_VERSION='\"${FIRMWARE_VERSION_STRING}\"' -include ${QUIET_H}"

  pio run -e "${pio_env}"

  python3 bin/uf2conv/uf2conv.py \
    ".pio/build/${pio_env}/firmware.hex" \
    -c \
    -o ".pio/build/${pio_env}/firmware.uf2" \
    -f 0xADA52840

  cp -f ".pio/build/${pio_env}/firmware.uf2" "out/${out_name}"
  cp -f ".pio/build/${pio_env}/firmware.uf2" "out/${versioned_name}"
  if [ ! -f ".pio/build/${pio_env}/firmware.zip" ]; then
    echo "ERROR: missing OTA firmware.zip for ${pio_env} (needed for Serial DFU)"
    exit 1
  fi
  cp -f ".pio/build/${pio_env}/firmware.zip" "out/${out_base}.zip"
  echo "Wrote out/${out_name}"
}

for role in "${ROLES[@]}"; do
  read -r pio_env role_slug <<<"$role"
  for variant in "${VARIANTS[@]}"; do
    read -r chem_macro chem_slug cells <<<"$variant"
    for protect in "${PROTECTS[@]}"; do
      read -r protect_macro protect_slug <<<"$protect"
      build_one "$pio_env" "$role_slug" "$chem_macro" "$chem_slug" "$cells" "$protect_macro" "$protect_slug"
    done
  done
done

echo "=== Darktec matrix done ==="
ls -la out/Darktec_*_*s_*.uf2

flasher_count="$(find out -maxdepth 1 -type f \( \
  -name 'Darktec_*_liion_1s_adc.uf2' -o \
  -name 'Darktec_*_liion_1s_off.uf2' -o \
  -name 'Darktec_*_lifepo4_1s_adc.uf2' -o \
  -name 'Darktec_*_lifepo4_1s_off.uf2' -o \
  -name 'Darktec_*_lto_1s_adc.uf2' -o \
  -name 'Darktec_*_lto_1s_off.uf2' -o \
  -name 'Darktec_*_lto_2s_adc.uf2' -o \
  -name 'Darktec_*_lto_2s_off.uf2' \
\) | wc -l | tr -d ' ')"

echo "Flasher UF2 count: ${flasher_count} (expected ${EXPECTED})"
if [ "${flasher_count}" -lt "${EXPECTED}" ]; then
  echo "ERROR: expected ${EXPECTED} flasher UF2s, got ${flasher_count}"
  ls -la out/
  exit 1
fi
