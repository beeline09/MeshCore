#!/usr/bin/env bash
# build-local.sh — build specified PlatformIO envs and produce all firmware types.
#
# Usage:
#   ./build-local.sh <env1> [env2] ...
#
# Output files land in out/ with names matching the GitHub Action convention:
#   <env>-<version>-<hash>.bin          (ESP32 update binary)
#   <env>-<version>-<hash>-merged.bin   (ESP32 full flash)
#   <env>-<version>-<hash>.uf2          (NRF52/RP2040 USB drag-and-drop)
#   <env>-<version>-<hash>.zip          (NRF52 BLE DFU OTA)
#
# Version is taken from the last git tag reachable from HEAD.
# Set DISABLE_DEBUG=1 to strip all debug flags.

set -euo pipefail

if [ $# -eq 0 ]; then
  echo "Usage: $0 <env1> [env2] ..."
  echo ""
  echo "Available companion envs:"
  pio project config 2>/dev/null | grep 'env:.*companion' | sed 's/env:/  /'
  exit 1
fi

# ── Version from last tag ─────────────────────────────────────────────────────
LAST_TAG=$(git describe --tags --abbrev=0 2>/dev/null || echo "v0.0.0")
# Strip everything up to and including the last '-' (removes prefixes like "companion-")
GIT_TAG_VERSION="${LAST_TAG##*-}"
COMMIT_HASH=$(git rev-parse --short HEAD)
FIRMWARE_VERSION_STRING="${GIT_TAG_VERSION}-${COMMIT_HASH}"

echo "Tag:     ${LAST_TAG}"
echo "Version: ${FIRMWARE_VERSION_STRING}"
echo ""

# ── Platform detection (reuses build.sh logic) ────────────────────────────────
PIO_CONFIG_JSON=$(pio project config --json-output 2>/dev/null)

get_platform_for_env() {
  echo "$PIO_CONFIG_JSON" | python3 -c "
import sys, json, re
data = json.load(sys.stdin)
for section, options in data:
    if section == 'env:$1':
        for key, value in options:
            if key == 'build_flags':
                for flag in value:
                    m = re.search(r'(ESP32_PLATFORM|NRF52_PLATFORM|STM32_PLATFORM|RP2040_PLATFORM)', flag)
                    if m:
                        print(m.group(1))
                        sys.exit(0)
"
}

# ── Debug flag stripping ──────────────────────────────────────────────────────
BASE_BUILD_FLAGS="${PLATFORMIO_BUILD_FLAGS:-}"
if [ "${DISABLE_DEBUG:-0}" = "1" ]; then
  BASE_BUILD_FLAGS="$BASE_BUILD_FLAGS -UMESH_DEBUG -UBLE_DEBUG_LOGGING -UWIFI_DEBUG_LOGGING -UBRIDGE_DEBUG -UGPS_NMEA_DEBUG -UCORE_DEBUG_LEVEL"
fi

mkdir -p out

# ── Build loop ────────────────────────────────────────────────────────────────
BUILT=()
FAILED=()

for ENV in "$@"; do
  echo "════════════════════════════════════════════════"
  echo "  ENV: $ENV"

  ENV_PLATFORM=$(get_platform_for_env "$ENV")
  echo "  Platform: ${ENV_PLATFORM:-unknown}"

  FILENAME="${ENV}-${FIRMWARE_VERSION_STRING}"
  BUILD_DIR=".pio/build/${ENV}"

  export PLATFORMIO_BUILD_FLAGS="${BASE_BUILD_FLAGS} -DFIRMWARE_VERSION='\"${FIRMWARE_VERSION_STRING}\"'"

  if pio run -e "$ENV"; then
    if [ "$ENV_PLATFORM" = "ESP32_PLATFORM" ]; then
      pio run -t mergebin -e "$ENV"
      cp "${BUILD_DIR}/firmware.bin"        "out/${FILENAME}.bin"        2>/dev/null || true
      cp "${BUILD_DIR}/firmware-merged.bin" "out/${FILENAME}-merged.bin" 2>/dev/null || true
      echo "  → out/${FILENAME}.bin"
      echo "  → out/${FILENAME}-merged.bin"

    elif [ "$ENV_PLATFORM" = "NRF52_PLATFORM" ]; then
      python3 bin/uf2conv/uf2conv.py "${BUILD_DIR}/firmware.hex" \
        -c -o "${BUILD_DIR}/firmware.uf2" -f 0xADA52840 \
        || echo "  WARNING: uf2conv failed — .uf2 skipped"
      cp "${BUILD_DIR}/firmware.uf2" "out/${FILENAME}.uf2" 2>/dev/null || true
      cp "${BUILD_DIR}/firmware.zip" "out/${FILENAME}.zip" 2>/dev/null || true
      echo "  → out/${FILENAME}.uf2"
      echo "  → out/${FILENAME}.zip"

    elif [ "$ENV_PLATFORM" = "RP2040_PLATFORM" ]; then
      cp "${BUILD_DIR}/firmware.bin" "out/${FILENAME}.bin" 2>/dev/null || true
      cp "${BUILD_DIR}/firmware.uf2" "out/${FILENAME}.uf2" 2>/dev/null || true
      echo "  → out/${FILENAME}.bin"
      echo "  → out/${FILENAME}.uf2"

    elif [ "$ENV_PLATFORM" = "STM32_PLATFORM" ]; then
      cp "${BUILD_DIR}/firmware.bin" "out/${FILENAME}.bin" 2>/dev/null || true
      cp "${BUILD_DIR}/firmware.hex" "out/${FILENAME}.hex" 2>/dev/null || true
      echo "  → out/${FILENAME}.bin"
      echo "  → out/${FILENAME}.hex"

    else
      echo "  WARNING: unknown platform — copying firmware.bin as fallback"
      cp "${BUILD_DIR}/firmware.bin" "out/${FILENAME}.bin" 2>/dev/null || true
      echo "  → out/${FILENAME}.bin"
    fi

    BUILT+=("$ENV")
  else
    echo "  FAILED: $ENV"
    FAILED+=("$ENV")
  fi

  echo ""
done

# ── Summary ───────────────────────────────────────────────────────────────────
echo "════════════════════════════════════════════════"
echo "Built ${#BUILT[@]} / $# envs."
[ ${#FAILED[@]} -gt 0 ] && echo "Failed: ${FAILED[*]}"
echo ""
echo "Files in out/ for this build:"
ls -lh out/ | grep "${FIRMWARE_VERSION_STRING}" | awk '{printf "  %-8s %s\n", $5, $9}' || true
