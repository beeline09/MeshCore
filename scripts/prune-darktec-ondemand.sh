#!/usr/bin/env bash
# Prune old assets from the single cache Release tag `darktec-ondemand`.
#
# Env:
#   KEEP_DAYS   keep assets newer than this (default 14)
#   KEEP_MAX    max assets to keep after age filter (default 80 = ~40 builds)
#   GH_TOKEN    required
#   DRY_RUN=1   only print what would be deleted
set -euo pipefail

REPO="${GITHUB_REPOSITORY:-beeline09/MeshCore}"
TAG="${ONDEMAND_TAG:-darktec-ondemand}"
KEEP_DAYS="${KEEP_DAYS:-14}"
KEEP_MAX="${KEEP_MAX:-80}"
DRY_RUN="${DRY_RUN:-0}"

: "${GH_TOKEN:?GH_TOKEN required}"

export GH_TOKEN
export REPO TAG KEEP_DAYS KEEP_MAX DRY_RUN

python3 <<'PY'
import json, os, subprocess, sys
from datetime import datetime, timezone, timedelta

repo = os.environ["REPO"]
tag = os.environ["TAG"]
keep_days = int(os.environ["KEEP_DAYS"])
keep_max = int(os.environ["KEEP_MAX"])
dry = os.environ.get("DRY_RUN", "0") == "1"

def gh_json(args):
    out = subprocess.check_output(["gh", "api", *args], text=True)
    return json.loads(out)

# Resolve release by tag
rel = gh_json([f"repos/{repo}/releases/tags/{tag}"])
assets = rel.get("assets") or []
now = datetime.now(timezone.utc)
cutoff = now - timedelta(days=keep_days)

def updated(a):
    # GitHub: updated_at
    s = a.get("updated_at") or a.get("created_at")
    return datetime.fromisoformat(s.replace("Z", "+00:00"))

# Sort newest first
assets_sorted = sorted(assets, key=updated, reverse=True)

to_delete = []
kept = []
for a in assets_sorted:
    if updated(a) < cutoff:
        to_delete.append(a)
        continue
    if len(kept) >= keep_max:
        to_delete.append(a)
        continue
    kept.append(a)

print(f"ondemand prune: total={len(assets)} keep={len(kept)} delete={len(to_delete)} "
      f"(KEEP_DAYS={keep_days} KEEP_MAX={keep_max})")

for a in to_delete:
    aid = a["id"]
    name = a["name"]
    when = a.get("updated_at") or a.get("created_at")
    print(f"  delete {name} ({when})")
    if dry:
        continue
    subprocess.check_call(["gh", "api", "-X", "DELETE", f"repos/{repo}/releases/assets/{aid}"])

if dry:
    print("DRY_RUN=1 — nothing deleted")
PY
