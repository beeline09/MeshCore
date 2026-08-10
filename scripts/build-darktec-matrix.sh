#!/usr/bin/env bash
# Build Darktec companion+repeater UF2 matrix (chemistry × cells) into out/.
# Filenames match the flasher site:
#   Darktec_companion_radio_ble_{liion|lifepo4|lto}_{1|2}s.uf2
#   Darktec_repeater_{liion|lifepo4|lto}_{1|2}s.uf2
#
# Requires: PlatformIO, FIRMWARE_VERSION (e.g. v0.0.0 or darktec-dev)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if [ -z "${FIRMWARE_VERSION:-}" ]; then
  echo "FIRMWARE_VERSION must be set"
  exit 1
fi

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

  echo "=== Building ${out_name} (${pio_env}, ${chem_macro}, cells=${cells}) ==="

  # -U then -D so matrix overrides win over variants/darktec/platformio.ini defaults.
  # Keep CFG_DEBUG defined (nRF52 core needs it); quiet MeshCore debug macros for release builds.
  export PLATFORMIO_BUILD_FLAGS="-UBATTERY_CHEMISTRY -UBATTERY_CELLS -DBATTERY_CHEMISTRY=${chem_macro} -DBATTERY_CELLS=${cells} -UMESH_DEBUG -UBLE_DEBUG_LOGGING -DCFG_DEBUG=0"

  # Reuse project build.sh packaging (versioned name + uf2conv).
  /usr/bin/env bash build.sh build-firmware "${pio_env}"

  local commit
  commit="$(git rev-parse --short HEAD)"
  local versioned="${pio_env}-${FIRMWARE_VERSION}-${commit}.uf2"
  if [ ! -f "out/${versioned}" ]; then
    echo "ERROR: expected out/${versioned}"
    ls -la out/ || true
    exit 1
  fi

  cp -f "out/${versioned}" "out/${out_name}"
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
