#!/usr/bin/env bash
# Build a single Darktec firmware variant (on-demand / custom advert name / radio).
#
# Required env:
#   FIRMWARE_VERSION   e.g. v1.16b3
#   DARKTEC_PIO_ENV    e.g. Darktec_repeater
#   DARKTEC_ROLE_SLUG  e.g. repeater
#   BATTERY_CHEMISTRY  e.g. BATTERY_CHEM_LIION
#   BATTERY_CELLS      e.g. 1
#   DARKTEC_BATT_PROTECT  e.g. DARKTEC_BATT_PROTECT_ADC
#   CHEM_SLUG          e.g. liion
#   PROTECT_SLUG       e.g. adc
#
# Optional:
#   ADVERT_NAME        node name string (default depends on role / platformio.ini)
#   NAME_SLUG          sanitized slug for filename (default: default)
#   LORA_FREQ          MHz (default 869.075)
#   LORA_BW            kHz (default 62.5)
#   LORA_SF            5–12 (default 8)
#   LORA_CR            5–8 (default 8)
#   LORA_TX_POWER      dBm (default 22)
#
# Output:
#   out/Darktec_{role}_{chem}_{cells}s_{protect}__{name_slug}__{radio_slug}__{sha}.{uf2,zip}
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

: "${FIRMWARE_VERSION:?}"
: "${DARKTEC_PIO_ENV:?}"
: "${DARKTEC_ROLE_SLUG:?}"
: "${BATTERY_CHEMISTRY:?}"
: "${BATTERY_CELLS:?}"
: "${DARKTEC_BATT_PROTECT:?}"
: "${CHEM_SLUG:?}"
: "${PROTECT_SLUG:?}"

COMMIT_HASH="$(git rev-parse --short=8 HEAD)"
FIRMWARE_VERSION_STRING="${FIRMWARE_VERSION}-${COMMIT_HASH}"
NAME_SLUG="${NAME_SLUG:-default}"
NAME_SLUG="$(echo "$NAME_SLUG" | tr '[:upper:]' '[:lower:]' | sed -E 's/[^a-z0-9]+/-/g; s/^-+//; s/-+$//; s/^$/default/' | cut -c1-24)"

LORA_FREQ="${LORA_FREQ:-869.075}"
LORA_BW="${LORA_BW:-62.5}"
LORA_SF="${LORA_SF:-8}"
LORA_CR="${LORA_CR:-8}"
LORA_TX_POWER="${LORA_TX_POWER:-22}"

# Filename-safe radio fingerprint (dots → p)
radio_token() {
  printf '%s' "$1" | tr '.' 'p' | tr -cd 'A-Za-z0-9p-'
}
RADIO_SLUG="f$(radio_token "$LORA_FREQ")-bw$(radio_token "$LORA_BW")-sf${LORA_SF}-cr${LORA_CR}-tx${LORA_TX_POWER}"

OUT_BASE="Darktec_${DARKTEC_ROLE_SLUG}_${CHEM_SLUG}_${BATTERY_CELLS}s_${PROTECT_SLUG}__${NAME_SLUG}__${RADIO_SLUG}__${COMMIT_HASH}"

rm -rf out
mkdir -p out

EXTRA_ADVERT=""
if [ -n "${ADVERT_NAME:-}" ]; then
  # Strip CR/LF/quotes; max 31 chars (MeshCore name field)
  SAFE_NAME="$(printf '%s' "$ADVERT_NAME" | tr -d '\r\n"'\''' | cut -c1-31)"
  EXTRA_ADVERT=" -UADVERT_NAME -DADVERT_NAME='\"${SAFE_NAME}\"'"
fi

EXTRA_RADIO=" -DDARKTEC_RADIO_CUSTOM -ULORA_FREQ -DLORA_FREQ=${LORA_FREQ} -ULORA_BW -DLORA_BW=${LORA_BW} -ULORA_SF -DLORA_SF=${LORA_SF} -ULORA_CR -DLORA_CR=${LORA_CR} -ULORA_TX_POWER -DLORA_TX_POWER=${LORA_TX_POWER}"

echo "=== On-demand build ${OUT_BASE} ==="
echo "env=${DARKTEC_PIO_ENV} chem=${BATTERY_CHEMISTRY} cells=${BATTERY_CELLS} protect=${DARKTEC_BATT_PROTECT} name=${ADVERT_NAME:-<default>}"
echo "radio freq=${LORA_FREQ} bw=${LORA_BW} sf=${LORA_SF} cr=${LORA_CR} tx=${LORA_TX_POWER}"

export PLATFORMIO_BUILD_FLAGS="-UBATTERY_CHEMISTRY -UBATTERY_CELLS -UDARKTEC_BATT_PROTECT -DBATTERY_CHEMISTRY=${BATTERY_CHEMISTRY} -DBATTERY_CELLS=${BATTERY_CELLS} -DDARKTEC_BATT_PROTECT=${DARKTEC_BATT_PROTECT} -UMESH_DEBUG -UBLE_DEBUG_LOGGING -DCFG_DEBUG=0 -DFIRMWARE_VERSION='\"${FIRMWARE_VERSION_STRING}\"'${EXTRA_ADVERT}${EXTRA_RADIO}"

pio run -e "${DARKTEC_PIO_ENV}"

python3 bin/uf2conv/uf2conv.py \
  ".pio/build/${DARKTEC_PIO_ENV}/firmware.hex" \
  -c \
  -o ".pio/build/${DARKTEC_PIO_ENV}/firmware.uf2" \
  -f 0xADA52840

cp -f ".pio/build/${DARKTEC_PIO_ENV}/firmware.uf2" "out/${OUT_BASE}.uf2"
if [ ! -f ".pio/build/${DARKTEC_PIO_ENV}/firmware.zip" ]; then
  echo "ERROR: missing OTA firmware.zip"
  exit 1
fi
cp -f ".pio/build/${DARKTEC_PIO_ENV}/firmware.zip" "out/${OUT_BASE}.zip"

echo "Wrote out/${OUT_BASE}.uf2 and .zip"
ls -la out/
