#!/usr/bin/env bash
# Build Darktec companion+repeater UF2 matrix (chemistry × cells) into out/.
# Filenames match the flasher site:
#   Darktec_companion_radio_ble_{liion|lifepo4|lto}_{1|2}s.uf2
#   Darktec_repeater_{liion|lifepo4|lto}_{1|2}s.uf2
#
# NOTE: do NOT call build.sh per variant — it runs `rm -rf out` on every invoke.
#
# Requires: PlatformIO, FIRMWARE_VERSION (e.g. darktec-1)
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

# chem_macro chem_slug cells
VARIANTS=(
  "BATTERY_CHEM_LIION liion 1"
  "BATTERY_CHEM_LIFEPO4 lifepo4 1"
  "BATTERY_CHEM_LTO lto 1"
  "BATTERY_CHEM_LTO lto 2"
)

ROLES=(
  "Darktec_companion_radio_ble companion_radio_ble"
  "Darktec_repeater repeater"
)

build_one() {
  local pio_env="$1"
  local role_slug="$2"
  local chem_macro="$3"
  local chem_slug="$4"
  local cells="$5"
  local out_name="Darktec_${role_slug}_${chem_slug}_${cells}s.uf2"
  local versioned_name="${pio_env}-${FIRMWARE_VERSION_STRING}.uf2"

  echo "=== Building ${out_name} (${pio_env}, ${chem_macro}, cells=${cells}) ==="

  # -U/-D overrides win over variants/darktec/platformio.ini defaults.
  # Keep CFG_DEBUG defined (Adafruit nRF52 rtos.cpp requires it).
  export PLATFORMIO_BUILD_FLAGS="-UBATTERY_CHEMISTRY -UBATTERY_CELLS -DBATTERY_CHEMISTRY=${chem_macro} -DBATTERY_CELLS=${cells} -UMESH_DEBUG -UBLE_DEBUG_LOGGING -DCFG_DEBUG=0 -DFIRMWARE_VERSION='\"${FIRMWARE_VERSION_STRING}\"'"

  pio run -e "${pio_env}"

  python3 bin/uf2conv/uf2conv.py \
    ".pio/build/${pio_env}/firmware.hex" \
    -c \
    -o ".pio/build/${pio_env}/firmware.uf2" \
    -f 0xADA52840

  cp -f ".pio/build/${pio_env}/firmware.uf2" "out/${out_name}"
  cp -f ".pio/build/${pio_env}/firmware.uf2" "out/${versioned_name}"
  if [ -f ".pio/build/${pio_env}/firmware.zip" ]; then
    cp -f ".pio/build/${pio_env}/firmware.zip" "out/${pio_env}-${FIRMWARE_VERSION_STRING}.zip"
  fi
  echo "Wrote out/${out_name}"
}

for role in "${ROLES[@]}"; do
  read -r pio_env role_slug <<<"$role"
  for variant in "${VARIANTS[@]}"; do
    read -r chem_macro chem_slug cells <<<"$variant"
    build_one "$pio_env" "$role_slug" "$chem_macro" "$chem_slug" "$cells"
  done
done

echo "=== Darktec matrix done ==="
ls -la out/Darktec_*.uf2

count="$(find out -maxdepth 1 -name 'Darktec_*.uf2' ! -name '*-darktec-*.uf2' | wc -l | tr -d ' ')"
# flasher names only (exclude versioned copies that also match Darktec_*)
flasher_count="$(find out -maxdepth 1 -type f \( \
  -name 'Darktec_companion_radio_ble_*s.uf2' -o \
  -name 'Darktec_repeater_*s.uf2' \
\) | wc -l | tr -d ' ')"

echo "Flasher UF2 count: ${flasher_count}"
if [ "${flasher_count}" -lt 8 ]; then
  echo "ERROR: expected 8 flasher UF2s, got ${flasher_count}"
  ls -la out/
  exit 1
fi
