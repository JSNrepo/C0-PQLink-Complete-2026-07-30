#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
venv_dir=${C0PQLINK_QISKIT_VENV:-"$project_dir/.venv-qiskit"}

python3 -m venv "$venv_dir"
# shellcheck disable=SC1091
. "$venv_dir/bin/activate"
python -m pip install --upgrade pip
python -m pip install -r "$project_dir/quantum-tests/requirements.txt"

python "$project_dir/quantum-tests/qiskit_shor_rsa15.py" \
    --output "$project_dir/evidence/qiskit/shor-rsa15.json"
python "$project_dir/quantum-tests/qiskit_grover_toy_key.py" \
    --output "$project_dir/evidence/qiskit/grover-toy-key.json"
python "$project_dir/quantum-tests/qiskit_optimization_study.py" \
    --output "$project_dir/evidence/qiskit/optimization-study.json"

echo "Qiskit evidence regenerated under evidence/qiskit/"
