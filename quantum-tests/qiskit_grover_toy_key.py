#!/usr/bin/env python3
"""Reproducible Grover search over a deliberately tiny four-bit key space."""

from __future__ import annotations

import argparse
import json
import math
import platform
from pathlib import Path
from typing import Any

import qiskit
import qiskit_aer
from qiskit import QuantumCircuit, transpile
from qiskit_aer import AerSimulator


KEY_BITS = 4
TARGET_KEY = 0b1011


def mark_target(circuit: QuantumCircuit, target: int) -> None:
    """Apply a phase flip only to the computational basis state `target`."""
    qubits = list(range(KEY_BITS))
    for qubit in qubits:
        if ((target >> qubit) & 1) == 0:
            circuit.x(qubit)
    circuit.h(qubits[-1])
    circuit.mcx(qubits[:-1], qubits[-1])
    circuit.h(qubits[-1])
    for qubit in qubits:
        if ((target >> qubit) & 1) == 0:
            circuit.x(qubit)


def diffuse(circuit: QuantumCircuit) -> None:
    qubits = list(range(KEY_BITS))
    circuit.h(qubits)
    circuit.x(qubits)
    circuit.h(qubits[-1])
    circuit.mcx(qubits[:-1], qubits[-1])
    circuit.h(qubits[-1])
    circuit.x(qubits)
    circuit.h(qubits)


def build_circuit(iterations: int) -> QuantumCircuit:
    circuit = QuantumCircuit(KEY_BITS, KEY_BITS)
    circuit.h(range(KEY_BITS))
    for _ in range(iterations):
        mark_target(circuit, TARGET_KEY)
        diffuse(circuit)
    circuit.measure(range(KEY_BITS), range(KEY_BITS))
    return circuit


def run_experiment(shots: int, seed: int) -> dict[str, Any]:
    search_space = 2**KEY_BITS
    iterations = math.floor((math.pi / 4) * math.sqrt(search_space))
    circuit = build_circuit(iterations)
    simulator = AerSimulator(method="statevector", seed_simulator=seed)
    compiled = transpile(
        circuit,
        simulator,
        optimization_level=1,
        seed_transpiler=seed,
    )
    counts = simulator.run(compiled, shots=shots).result().get_counts()
    target_bits = format(TARGET_KEY, f"0{KEY_BITS}b")
    target_hits = int(counts.get(target_bits, 0))

    return {
        "experiment": "grover_search_toy_4_bit_key",
        "scope_warning": (
            "Educational reduced-key experiment only; this is not an attack "
            "on a 128-bit Ascon key or a 256-bit ML-KEM shared secret."
        ),
        "versions": {
            "python": platform.python_version(),
            "qiskit": qiskit.__version__,
            "qiskit_aer": qiskit_aer.__version__,
        },
        "configuration": {
            "shots": shots,
            "seed_simulator": seed,
            "seed_transpiler": seed,
            "key_bits": KEY_BITS,
            "target_key_binary": target_bits,
            "search_space": search_space,
            "grover_iterations": iterations,
            "classical_worst_case_oracle_queries": search_space,
            "classical_average_oracle_queries": (search_space + 1) / 2,
        },
        "circuit": {
            "qubits": compiled.num_qubits,
            "classical_bits": compiled.num_clbits,
            "depth": compiled.depth(),
            "size": compiled.size(),
            "operation_counts": {
                str(name): int(value)
                for name, value in compiled.count_ops().items()
            },
        },
        "raw_counts": {
            str(bits): int(value)
            for bits, value in sorted(counts.items())
        },
        "result": {
            "target_hits": target_hits,
            "target_probability": target_hits / shots,
            "most_frequent_key": max(counts, key=counts.get),
            "attack_succeeded": max(counts, key=counts.get) == target_bits,
        },
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--shots", type=int, default=4096)
    parser.add_argument("--seed", type=int, default=20260730)
    parser.add_argument("--output", type=Path)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.shots <= 0:
        raise SystemExit("--shots must be positive")
    result = run_experiment(args.shots, args.seed)
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    print(rendered, end="")


if __name__ == "__main__":
    main()
