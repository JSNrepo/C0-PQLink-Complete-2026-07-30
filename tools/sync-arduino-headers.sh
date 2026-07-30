#!/usr/bin/env sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$project_dir/src/c0pqlink"
cp "$project_dir"/include/c0pqlink/*.h "$project_dir/src/c0pqlink/"
