#!/usr/bin/env bash
# Human-readable Darktec changelog: only beeline09 changes since PREV_TAG.
# Upstream (Rogovogor south_edition) is referenced by link, not listed here.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

PREV_TAG="${PREV_TAG:-}"
TAG_END="${TAG_END:-HEAD}"   # конец диапазона; для исторической регенерации — сам тег
VERSION="${VERSION:-unknown}"
OUT="${1:-/tmp/darktec-changelog.md}"
UPSTREAM_BRANCH_URL="https://github.com/rogovogor/MeshCore/tree/south_edition"

if [ -n "$PREV_TAG" ] && ! git rev-parse "$PREV_TAG" >/dev/null 2>&1; then
  git fetch --tags --depth=80 origin "refs/tags/${PREV_TAG}:refs/tags/${PREV_TAG}" 2>/dev/null || true
fi

PATHS=(
  variants/darktec
  examples/companion_radio
  scripts/darktec-changelog.sh
  scripts/build-darktec-ondemand.sh
  scripts/build-darktec-matrix.sh
  scripts/next-darktec-version.sh
  .github/workflows/build-darktec-firmwares.yml
  .github/workflows/build-darktec-ondemand.yml
  docs/changes
)

collect_log() {
  local range="$1"
  # только свои коммиты: фильтр по автору отсекает апстрим (говогор),
  # который тоже трогает examples/companion_radio
  {
    git log --no-merges --pretty=format:'%s' --author='Rasul Ismailov' --author='beeline09' "$range" -- "${PATHS[@]}" 2>/dev/null || true
    echo
    git log --no-merges --pretty=format:'%s' --author='Rasul Ismailov' --author='beeline09' "$range" \
      --grep='[Dd]arktec|[Oo]n-?demand|DARKTEC_|beeline09|Serial DFU web flasher' -i -E \
      2>/dev/null || true
  }
}

RANGE=""
if [ -n "$PREV_TAG" ] && git rev-parse "$PREV_TAG" >/dev/null 2>&1; then
  RANGE="${PREV_TAG}..${TAG_END}"
elif [ -n "${TAG_END:-}" ] && git rev-parse "$TAG_END" >/dev/null 2>&1; then
  RANGE="${TAG_END}"   # первый релиз: вся история до тега (ограничено -40/-30)
fi
if [ -n "$RANGE" ]; then
  collect_log "$RANGE" > /tmp/darktec-commits.txt
else
  # First versioned release: Darktec-related commits only
  {
    git log --no-merges --pretty=format:'%s' --author='Rasul Ismailov' --author='beeline09' -- "${PATHS[@]}" -40 2>/dev/null || true
    echo
    git log --no-merges --pretty=format:'%s' --author='Rasul Ismailov' --author='beeline09' \
      --grep='[Dd]arktec|[Oo]n-?demand|DARKTEC_|beeline09' -i -E -30 \
      2>/dev/null || true
  } > /tmp/darktec-commits.txt
fi

cat /tmp/darktec-commits.txt | \
  PREV_TAG="$PREV_TAG" VERSION="$VERSION" OUT="$OUT" UPSTREAM_BRANCH_URL="$UPSTREAM_BRANCH_URL" python3 -c '
import os, re, sys
from collections import defaultdict

version = os.environ.get("VERSION", "unknown")
prev = os.environ.get("PREV_TAG") or "начало"
out_path = os.environ["OUT"]
upstream = os.environ.get("UPSTREAM_BRANCH_URL") or \
    "https://github.com/rogovogor/MeshCore/tree/south_edition"
lines = [ln.strip() for ln in sys.stdin.read().splitlines() if ln.strip()]

buckets = defaultdict(list)
skip_re = re.compile(
    r"^(merge|Merge|ci\(darktec\): share concurrency|ci\(darktec\): dispatch Pages)",
    re.I,
)
ours_re = re.compile(
    r"darktec|on-?demand|beeline09|DARKTEC_|Serial DFU|web flasher|"
    r"кириллиц|зарядк|защит\w*\s+бата|battery protect|adc/?off|"
    r"ADVERT_NAME|advert_name|LORA_\*|OLED от|OLED и не гасить|"
    r"VBAT|SoftDevice|EU868|human changelog|Versioned releases|"
    r"Darktec UF2|UF2 matrix|матриц|Discover|гиберн|SYSTEMOFF",
    re.I,
)

def classify(msg: str):
    m = re.match(r"^(\w+)(\([^)]+\))?:\s*(.+)$", msg)
    if not m:
        return "Прочее", msg
    kind, rest = m.group(1).lower(), m.group(3)
    mapping = {
        "darktec": "Новое",
        "feat": "Новое",
        "feature": "Новое",
        "fix": "Исправления",
        "bugfix": "Исправления",
        "docs": "Документация",
        "doc": "Документация",
        "refactor": "Рефакторинг",
        "perf": "Производительность",
        "ci": "Сборка / CI",
        "chore": "Обслуживание",
        "build": "Сборка / CI",
        "style": "Обслуживание",
        "test": "Тесты",
    }
    text = rest[:1].upper() + rest[1:] if rest else msg
    # Никаких следов ИИ-агента в changelog: срезаем «(spec NNNN…)» и подобное.
    text = re.sub(r"\s*\((?:spec|SDD|спека?)\s[^)]*\)", "", text, flags=re.I)
    text = re.sub(r"\s*\((?:spec|SDD|спека?)\d+[^)]*\)", "", text, flags=re.I)
    text = re.sub(r"\s+\(\d{4}-\d{2}-\d{2}\)$", "", text)
    text = text.strip()
    return mapping.get(kind, "Прочее"), text

seen = set()
for msg in lines:
    if skip_re.search(msg):
        continue
    if not ours_re.search(msg):
        continue
    key = msg.lower()
    if key in seen:
        continue
    seen.add(key)
    cat, text = classify(msg)
    if "github-actions" in text.lower():
        continue
    buckets[cat].append(text)

order = ["Новое", "Исправления", "Сборка / CI", "Документация", "Рефакторинг",
         "Производительность", "Тесты", "Обслуживание", "Прочее"]

parts = [
    f"## Darktec {version}",
    "",
    f"Прошивка основана на изменениях ветки "
    f"[south_edition]({upstream}) (Rogovogor / MeshCore). "
    f"Полный базовый changelog смотрите там.",
    "",
    f"### Изменения beeline09 с `{prev}`",
    "",
]
any_item = False
for cat in order:
    items = buckets.get(cat) or []
    if not items:
        continue
    any_item = True
    parts.append(f"#### {cat}")
    for it in items[:40]:
        parts.append(f"- {it}")
    parts.append("")

if not any_item:
    parts.append("_Нет пользовательских коммитов beeline09/Darktec в диапазоне — технический релиз._")
    parts.append("")

parts.extend([
    "---",
    "",
    "Community-сборка **beeline09**. Flasher: https://beeline09.github.io/darktec/",
    "",
])

text = "\n".join(parts)
open(out_path, "w", encoding="utf-8").write(text)
print(text)
'
