#!/bin/sh
set -eu

PROJECT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$PROJECT_ROOT"

npm ci --ignore-scripts --cache .npm-cache
make clean
make verify
make sanitizer-test
make benchmark
make build/avr-lms-w4/factory-reset.eep
python3 tools/check_release.py

printf '%s\n' \
  "PASS: clean package rebuilt, tested, simulated, benchmarked, and gated"
