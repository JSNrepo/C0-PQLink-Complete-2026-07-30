#!/usr/bin/env python3
"""Reproducible circuit-optimization study for the phase-one review.

The study measures Qiskit transpiler optimization levels for the existing toy
Shor/RSA-15 and Grover/four-bit demonstrations.  It also sweeps the Grover
iteration count to show the measured amplitude-amplification optimum.

This is a study of small educational circuits.  It is not a resource estimate
for attacking production RSA, ECC, ML-KEM, or Ascon.
"""

from __future__ import annotations

import argparse
import json
import platform
import time
from pathlib import Path
from typing import Any, Callable

import qiskit
import qiskit_aer
from qiskit import QuantumCircuit, transpile
from qiskit_aer import AerSimulator

from qiskit_grover_toy_key import (
    TARGET_KEY,
    build_circuit as build_grover_circuit,
)
from qiskit_shor_rsa15 import (
    build_order_finding_circuit,
    factors_from_phase_bits,
)


BASIS_GATES = ["rz", "sx", "x", "cx"]
OPTIMIZATION_LEVELS = [0, 1, 2, 3]
GROVER_ITERATIONS = list(range(6))


def operation_width_counts(circuit: QuantumCircuit) -> dict[str, int]:
    """Count operations by arity after transpilation."""
    counts = {
        "one_qubit": 0,
        "two_qubit": 0,
        "three_or_more_qubit": 0,
        "measurement": 0,
    }
    for instruction in circuit.data:
        operation = instruction.operation
        if operation.name == "measure":
            counts["measurement"] += 1
        elif operation.num_qubits == 1:
            counts["one_qubit"] += 1
        elif operation.num_qubits == 2:
            counts["two_qubit"] += 1
        elif operation.num_qubits >= 3:
            counts["three_or_more_qubit"] += 1
    return counts


def circuit_metrics(circuit: QuantumCircuit) -> dict[str, Any]:
    return {
        "qubits": circuit.num_qubits,
        "classical_bits": circuit.num_clbits,
        "depth": circuit.depth(),
        "size": circuit.size(),
        "operation_counts": {
            str(name): int(value)
            for name, value in circuit.count_ops().items()
        },
        "operation_width_counts": operation_width_counts(circuit),
    }


def sorted_counts(counts: dict[str, int]) -> dict[str, int]:
    return {
        str(bits): int(value)
        for bits, value in sorted(counts.items())
    }


def shor_success(counts: dict[str, int], shots: int) -> dict[str, Any]:
    successful_shots = 0
    recovered: dict[str, int] = {}
    for bits, occurrences in counts.items():
        candidate = factors_from_phase_bits(bits.replace(" ", ""))
        if candidate is None:
            continue
        order, p, q = candidate
        key = f"order={order};factors={p}x{q}"
        recovered[key] = recovered.get(key, 0) + int(occurrences)
        successful_shots += int(occurrences)
    return {
        "successful_factor_shots": successful_shots,
        "successful_factor_probability": successful_shots / shots,
        "recovered_candidates": recovered,
    }


def grover_success(
    counts: dict[str, int],
    shots: int,
) -> dict[str, Any]:
    target_bits = format(TARGET_KEY, "04b")
    target_hits = int(counts.get(target_bits, 0))
    return {
        "target_key_binary": target_bits,
        "target_hits": target_hits,
        "target_probability": target_hits / shots,
        "most_frequent_key": max(counts, key=counts.get),
    }


def compile_and_run(
    circuit: QuantumCircuit,
    simulator: AerSimulator,
    optimization_level: int,
    shots: int,
    seed: int,
    evaluate: Callable[[dict[str, int], int], dict[str, Any]],
) -> dict[str, Any]:
    compile_start = time.perf_counter_ns()
    compiled = transpile(
        circuit,
        basis_gates=BASIS_GATES,
        optimization_level=optimization_level,
        seed_transpiler=seed,
    )
    compile_ns = time.perf_counter_ns() - compile_start

    simulation_start = time.perf_counter_ns()
    counts = simulator.run(
        compiled,
        shots=shots,
        seed_simulator=seed,
    ).result().get_counts()
    simulation_ns = time.perf_counter_ns() - simulation_start

    return {
        "optimization_level": optimization_level,
        "compile_time_ns_observed": compile_ns,
        "simulation_time_ns_observed": simulation_ns,
        "circuit": circuit_metrics(compiled),
        "raw_counts": sorted_counts(counts),
        "result": evaluate(counts, shots),
    }


def optimization_comparison(
    simulator: AerSimulator,
    shots: int,
    seed: int,
) -> dict[str, Any]:
    circuits = {
        "compiled_shor_order_finding_toy_rsa_15": (
            build_order_finding_circuit(),
            shor_success,
        ),
        "grover_search_toy_4_bit_key_three_iterations": (
            build_grover_circuit(3),
            grover_success,
        ),
    }
    output: dict[str, Any] = {}
    for name, (logical, evaluate) in circuits.items():
        output[name] = {
            "logical_circuit": circuit_metrics(logical),
            "transpiled_runs": [
                compile_and_run(
                    logical,
                    simulator,
                    level,
                    shots,
                    seed,
                    evaluate,
                )
                for level in OPTIMIZATION_LEVELS
            ],
        }
    return output


def grover_iteration_sweep(
    simulator: AerSimulator,
    shots: int,
    seed: int,
) -> list[dict[str, Any]]:
    output = []
    for iterations in GROVER_ITERATIONS:
        circuit = build_grover_circuit(iterations)
        run = compile_and_run(
            circuit,
            simulator,
            optimization_level=3,
            shots=shots,
            seed=seed + iterations,
            evaluate=grover_success,
        )
        run["iterations"] = iterations
        run["seed_simulator"] = seed + iterations
        run["seed_transpiler"] = seed + iterations
        output.append(run)
    return output


def run_study(shots: int, seed: int) -> dict[str, Any]:
    simulator = AerSimulator(method="statevector")
    return {
        "experiment": "qiskit_circuit_optimization_study",
        "scope_warning": (
            "Small educational circuit optimization only. These runs do not "
            "break or prove the security of RSA-2048, ECC, ML-KEM, or Ascon."
        ),
        "versions": {
            "python": platform.python_version(),
            "qiskit": qiskit.__version__,
            "qiskit_aer": qiskit_aer.__version__,
        },
        "platform": {
            "sdk": "Qiskit",
            "simulator": "Qiskit Aer",
            "backend": "AerSimulator",
            "method": "statevector",
            "basis_gates": BASIS_GATES,
            "is_quantum_hardware": False,
        },
        "configuration": {
            "shots_per_run": shots,
            "base_seed": seed,
            "optimization_levels": OPTIMIZATION_LEVELS,
            "grover_iteration_sweep": GROVER_ITERATIONS,
            "timing_note": (
                "Observed wall-clock compile/simulation times are environment "
                "dependent; circuit metrics and seeded counts are the primary "
                "reproducible evidence."
            ),
        },
        "optimization_comparison": optimization_comparison(
            simulator,
            shots,
            seed,
        ),
        "grover_iteration_sweep": grover_iteration_sweep(
            simulator,
            shots,
            seed,
        ),
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
    result = run_study(args.shots, args.seed)
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    print(rendered, end="")


if __name__ == "__main__":
    main()
