#!/usr/bin/env bash
# Compute next Darktec semver: v{BASE}b{N}
# BASE from companion MyMesh.h (Rogovogor / south major), N increments per Darktec release tag.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MESH_H="${ROOT}/examples/companion_radio/MyMesh.h"
REPO="${FIRMWARE_REPO:-beeline09/MeshCore}"
API="${GITHUB_API_URL:-https://api.github.com}"

extract_base() {
  local line
  line="$(grep -E '#define[[:space:]]+FIRMWARE_VERSION' "$MESH_H" | head -1 || true)"
  if [[ "$line" =~ v([0-9]+\.[0-9]+\.[0-9]+) ]]; then
    echo "${BASH_REMATCH[1]}"
    return
  fi
  echo "0.0.0"
}

BASE="$(extract_base)"
TOKEN="${GITHUB_TOKEN:-${GH_TOKEN:-}}"

auth_args=()
if [ -n "$TOKEN" ]; then
  auth_args=(-H "Authorization: Bearer ${TOKEN}")
fi

tags_json="$(curl -fsSL \
  -H "Accept: application/vnd.github+json" \
  -H "User-Agent: darktec-version" \
  -H "X-GitHub-Api-Version: 2022-11-28" \
  "${auth_args[@]}" \
  "${API}/repos/${REPO}/tags?per_page=100" || echo '[]')"

eval "$(echo "$tags_json" | BASE="$BASE" python3 -c '
import json, os, re, sys
base = os.environ["BASE"]
data = json.load(sys.stdin)
pat_base = re.compile(rf"^darktec-v{re.escape(base)}b(\d+)$")
mx = 0
prev = ""
for t in data:
    name = t.get("name", "")
    m = pat_base.match(name)
    if m:
        n = int(m.group(1))
        if n >= mx:
            mx = n
            prev = name
print(f"MAX_N={mx}")
print(f"PREV_TAG={prev}")
')"

NEXT=$((MAX_N + 1))
VERSION="v${BASE}b${NEXT}"
TAG="darktec-${VERSION}"

{
  echo "BASE=${BASE}"
  echo "BUILD=${NEXT}"
  echo "VERSION=${VERSION}"
  echo "TAG=${TAG}"
  echo "PREV_TAG=${PREV_TAG}"
} | tee /tmp/darktec-version.env

if [ -n "${GITHUB_ENV:-}" ]; then
  cat /tmp/darktec-version.env >> "$GITHUB_ENV"
fi

# Job outputs for matrix workflows (version → build/release).
if [ -n "${GITHUB_OUTPUT:-}" ]; then
  cat /tmp/darktec-version.env >> "$GITHUB_OUTPUT"
fi

echo "Darktec version → ${VERSION} (tag ${TAG}, prev ${PREV_TAG:-none})"
