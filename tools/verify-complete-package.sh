#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$project_dir"

if ! command -v make >/dev/null 2>&1; then
    echo "Missing required command: make" >&2
    exit 1
fi
if ! command -v npm >/dev/null 2>&1; then
    echo "Missing required command: npm" >&2
    exit 1
fi
if ! command -v python3 >/dev/null 2>&1; then
    echo "Missing required command: python3" >&2
    exit 1
fi

if [ ! -d node_modules/@noble/post-quantum ]; then
    npm_cache_dir=${C0PQLINK_NPM_CACHE:-"$project_dir/.npm-cache"}
    npm ci --cache "$npm_cache_dir"
fi

make verify
make size-report

python3 - <<'PY'
import json
from pathlib import Path

root = Path.cwd()
python_sources = [
    root / "micropython" / "c0pqlink.py",
    *sorted((root / "quantum-tests").glob("*.py")),
]
for path in python_sources:
    compile(path.read_text(encoding="utf-8"), str(path), "exec")

json_evidence = sorted((root / "evidence" / "qiskit").glob("*.json"))
if not json_evidence:
    raise SystemExit("No Qiskit JSON evidence files found")
for path in json_evidence:
    with path.open("r", encoding="utf-8") as handle:
        json.load(handle)

print(
    "Python/evidence validation passed: "
    f"{len(python_sources)} source files, {len(json_evidence)} JSON files"
)
PY

echo "C0-PQLink complete-package verification passed"
