#!/usr/bin/env bash
# Human-readable Darktec official changelog: only beeline09 variant changes.
# Upstream (meshcore-dev MeshCore `dev`) is referenced by link, not listed here.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

VERSION="${VERSION:-unknown}"
OUT="${1:-/tmp/darktec-official-changelog.md}"
UPSTREAM_BRANCH_URL="https://github.com/meshcore-dev/MeshCore/tree/dev"
VARIANT_URL="https://github.com/beeline09/MeshCore/tree/dev-darktec"

PATHS=(
  variants/darktec
  scripts/darktec-official-changelog.sh
  scripts/build-darktec-official-matrix.sh
  scripts/build-darktec-ondemand.sh
  .github/workflows/build-darktec-official-firmwares.yml
)

{
  git log --no-merges --pretty=format:'%s' -- "${PATHS[@]}" -40 2>/dev/null || true
  echo
  git log --no-merges --pretty=format:'%s' \
    --grep='[Dd]arktec|[Oo]n-?demand|DARKTEC_|beeline09' -i -E -30 \
    2>/dev/null || true
} > /tmp/darktec-official-commits.txt

cat /tmp/darktec-official-commits.txt | \
  VERSION="$VERSION" OUT="$OUT" UPSTREAM_BRANCH_URL="$UPSTREAM_BRANCH_URL" \
  VARIANT_URL="$VARIANT_URL" python3 -c '
import os, re, sys
from collections import defaultdict

version = os.environ.get("VERSION", "unknown")
out_path = os.environ["OUT"]
upstream = os.environ.get("UPSTREAM_BRANCH_URL") or \
    "https://github.com/meshcore-dev/MeshCore/tree/dev"
variant = os.environ.get("VARIANT_URL") or \
    "https://github.com/beeline09/MeshCore/tree/dev-darktec"
lines = [ln.strip() for ln in sys.stdin.read().splitlines() if ln.strip()]

buckets = defaultdict(list)
skip_re = re.compile(
    r"^(merge|Merge|ci\(darktec\): share concurrency|ci\(darktec\): dispatch Pages)",
    re.I,
)
ours_re = re.compile(
    r"darktec|on-?demand|beeline09|DARKTEC_|кириллиц|зарядк|"
    r"ADVERT_NAME|advert_name|LORA_\*|OLED|INA3221|battery chem|"
    r"Darktec UF2|UF2 matrix|матриц",
    re.I,
)

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
    f"## Darktec official {version}",
    "",
    f"Прошивка основана на ветке "
    f"[dev]({upstream}) (meshcore-dev / MeshCore). "
    f"Вариант платы: [dev-darktec]({variant}). "
    f"Это не бинарный релиз meshcore.io.",
    "",
    "### Изменения beeline09 / Darktec",
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
    parts.append("_Нет отдельных пользовательских коммитов варианта Darktec._")
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
