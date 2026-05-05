#!/usr/bin/env bash
# build-local.sh - build specified PlatformIO envs and collect firmware artifacts.
#
# Usage:
#   ./build-local.sh <env1> [env2] ...
#   ./build-local.sh env1,env2 env3
#
# Output files land in out/ with names matching the GitHub Action convention:
#   <env>-<version>-<hash>.bin          (update binary)
#   <env>-<version>-<hash>-merged.bin   (full flash image)
#   <env>-<version>-<hash>.uf2          (USB drag-and-drop)
#   <env>-<version>-<hash>.zip          (BLE DFU OTA)
#   <env>-<version>-<hash>.hex          (hex image, when produced)
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

trim_env_name() {
  local value="$1"
  value="${value#"${value%%[![:space:]]*}"}"
  value="${value%"${value##*[![:space:]]}"}"
  printf '%s' "$value"
}

ENVS=()
for ARG in "$@"; do
  IFS=',' read -ra PARTS <<< "$ARG"
  for PART in "${PARTS[@]}"; do
    ENV_NAME=$(trim_env_name "$PART")
    if [ -n "$ENV_NAME" ]; then
      ENVS+=("$ENV_NAME")
    fi
  done
done

# Version from last tag.
LAST_TAG=$(git describe --tags --abbrev=0 2>/dev/null || echo "v0.0.0")
# Strip everything up to and including the last '-' (removes prefixes like "companion-").
GIT_TAG_VERSION="${LAST_TAG##*-}"
COMMIT_HASH=$(git rev-parse --short HEAD)
FIRMWARE_VERSION_STRING="${GIT_TAG_VERSION}-${COMMIT_HASH}"

echo "Tag:     ${LAST_TAG}"
echo "Version: ${FIRMWARE_VERSION_STRING}"
echo ""

# Platform detection (reuses build.sh logic).
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

copy_artifact() {
  local source="$1"
  local dest="$2"
  if [ -f "$source" ]; then
    cp "$source" "$dest"
    echo "  -> $dest"
    return 0
  fi
  return 1
}

# Debug flag stripping.
BASE_BUILD_FLAGS="${PLATFORMIO_BUILD_FLAGS:-}"
if [ "${DISABLE_DEBUG:-0}" = "1" ]; then
  BASE_BUILD_FLAGS="$BASE_BUILD_FLAGS -UMESH_DEBUG -UBLE_DEBUG_LOGGING -UWIFI_DEBUG_LOGGING -UBRIDGE_DEBUG -UGPS_NMEA_DEBUG -UCORE_DEBUG_LEVEL"
fi

mkdir -p out

BUILT=()
FAILED=()

for ENV in "${ENVS[@]}"; do
  echo "================================================"
  echo "  ENV: $ENV"

  ENV_PLATFORM=$(get_platform_for_env "$ENV")
  echo "  Platform: ${ENV_PLATFORM:-unknown}"

  FILENAME="${ENV}-${FIRMWARE_VERSION_STRING}"
  BUILD_DIR=".pio/build/${ENV}"

  export PLATFORMIO_BUILD_FLAGS="${BASE_BUILD_FLAGS} -DFIRMWARE_VERSION='\"${FIRMWARE_VERSION_STRING}\"'"

  if pio run -e "$ENV"; then
    if [ "$ENV_PLATFORM" = "ESP32_PLATFORM" ]; then
      pio run -t mergebin -e "$ENV"
    elif [ "$ENV_PLATFORM" = "NRF52_PLATFORM" ]; then
      pio run -t create_uf2 -e "$ENV"
      if [ ! -f "${BUILD_DIR}/firmware.uf2" ] && [ -f "${BUILD_DIR}/firmware.hex" ]; then
        python3 bin/uf2conv/uf2conv.py "${BUILD_DIR}/firmware.hex" \
          -c -o "${BUILD_DIR}/firmware.uf2" -f 0xADA52840 \
          || echo "  WARNING: uf2conv failed; .uf2 skipped"
      fi
    fi

    COPIED=0
    copy_artifact "${BUILD_DIR}/firmware.bin"        "out/${FILENAME}.bin"        && COPIED=$((COPIED + 1)) || true
    copy_artifact "${BUILD_DIR}/firmware-merged.bin" "out/${FILENAME}-merged.bin" && COPIED=$((COPIED + 1)) || true
    copy_artifact "${BUILD_DIR}/firmware.uf2"        "out/${FILENAME}.uf2"        && COPIED=$((COPIED + 1)) || true
    copy_artifact "${BUILD_DIR}/firmware.zip"        "out/${FILENAME}.zip"        && COPIED=$((COPIED + 1)) || true
    copy_artifact "${BUILD_DIR}/firmware.hex"        "out/${FILENAME}.hex"        && COPIED=$((COPIED + 1)) || true

    if [ "$COPIED" -eq 0 ]; then
      echo "  WARNING: no firmware artifacts found in ${BUILD_DIR}"
    fi

    BUILT+=("$ENV")
  else
    echo "  FAILED: $ENV"
    FAILED+=("$ENV")
  fi

  echo ""
done

echo "================================================"
echo "Built ${#BUILT[@]} / ${#ENVS[@]} envs."
[ ${#FAILED[@]} -gt 0 ] && echo "Failed: ${FAILED[*]}"
echo ""
echo "Files in out/ for this build:"
ls -lh out/ | grep "${FIRMWARE_VERSION_STRING}" | awk '{printf "  %-8s %s\n", $5, $9}' || true
