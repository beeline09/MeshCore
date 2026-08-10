#!/usr/bin/env bash
# Human-readable changelog from git commits since PREV_TAG.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

PREV_TAG="${PREV_TAG:-}"
VERSION="${VERSION:-unknown}"
OUT="${1:-/tmp/darktec-changelog.md}"

if [ -n "$PREV_TAG" ] && ! git rev-parse "$PREV_TAG" >/dev/null 2>&1; then
  git fetch --tags --depth=80 origin "refs/tags/${PREV_TAG}:refs/tags/${PREV_TAG}" 2>/dev/null || true
fi

if [ -n "$PREV_TAG" ] && git rev-parse "$PREV_TAG" >/dev/null 2>&1; then
  RANGE="${PREV_TAG}..HEAD"
  git log --no-merges --pretty=format:'%s' "$RANGE" 2>/dev/null > /tmp/darktec-commits.txt || true
else
  # First versioned release: Darktec-related commits + recent tip
  {
    git log --no-merges --pretty=format:'%s' --grep='[Dd]arktec' -i -20 2>/dev/null || true
    echo
    git log --no-merges --pretty=format:'%s' -8 2>/dev/null || true
  } > /tmp/darktec-commits.txt
fi

cat /tmp/darktec-commits.txt | \
  PREV_TAG="$PREV_TAG" VERSION="$VERSION" OUT="$OUT" python3 -c '
import os, re, sys
from collections import defaultdict

version = os.environ.get("VERSION", "unknown")
prev = os.environ.get("PREV_TAG") or "начало"
out_path = os.environ["OUT"]
lines = [ln.strip() for ln in sys.stdin.read().splitlines() if ln.strip()]

buckets = defaultdict(list)
skip_re = re.compile(r"^(merge|Merge|ci\(darktec\): share concurrency)", re.I)

def classify(msg: str):
    m = re.match(r"^(\w+)(\([^)]+\))?:\s*(.+)$", msg)
    if not m:
        return "Прочее", msg
    kind, rest = m.group(1).lower(), m.group(3)
    mapping = {
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
    return mapping.get(kind, "Прочее"), text

seen = set()
for msg in lines:
    if skip_re.search(msg):
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

parts = [f"## Darktec {version}", "", f"Изменения с `{prev}`:", ""]
any_item = False
for cat in order:
    items = buckets.get(cat) or []
    if not items:
        continue
    any_item = True
    parts.append(f"### {cat}")
    for it in items[:40]:
        parts.append(f"- {it}")
    parts.append("")

if not any_item:
    parts.append("_Нет пользовательских коммитов в диапазоне — технический релиз._")
    parts.append("")

parts.extend([
    "---",
    "",
    "Community-сборка **beeline09** на базе south_edition (форк ветки Rogovogor / MeshCore).",
    "Flasher: https://beeline09.github.io/darktec/",
    "",
])

text = "\n".join(parts)
open(out_path, "w", encoding="utf-8").write(text)
print(text)
'
