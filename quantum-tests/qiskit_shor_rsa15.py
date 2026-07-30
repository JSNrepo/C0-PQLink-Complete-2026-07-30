#!/usr/bin/env python3
"""Reproducible Qiskit demonstration of Shor order finding against toy RSA-15.

This is intentionally a small, compiled demonstration circuit.  It proves that
the quantum order-finding workflow executes and recovers RSA-15's factors.  It
does not estimate or claim the ability to factor production RSA moduli.
"""

from __future__ import annotations

import argparse
import json
import math
import platform
from collections import Counter
from fractions import Fraction
from pathlib import Path
from typing import Any

import qiskit
import qiskit_aer
from qiskit import QuantumCircuit, transpile
from qiskit_aer import AerSimulator


RSA_N = 15
RSA_E = 3
RSA_MESSAGE = 2
ORDER_BASE = 2
COUNTING_QUBITS = 4
WORK_QUBITS = 4


def inverse_qft(circuit: QuantumCircuit, qubits: list[int]) -> None:
    """Append an inverse QFT without approximation."""
    count = len(qubits)
    for index in range(count // 2):
        circuit.swap(qubits[index], qubits[count - index - 1])
    for target in range(count):
        for control in range(target):
            angle = -math.pi / (2 ** (target - control))
            circuit.cp(angle, qubits[control], qubits[target])
        circuit.h(qubits[target])


def multiply_by_two_mod_15(power: int) -> QuantumCircuit:
    """Return the compiled permutation for multiplication by 2**power mod 15.

    This construction is specific to N=15.  Repeating the three swaps applies
    multiplication by two; powers are reduced modulo the order four.
    """
    permutation = QuantumCircuit(WORK_QUBITS, name=f"2^{power} mod 15")
    for _ in range(power % 4):
        permutation.swap(0, 1)
        permutation.swap(1, 2)
        permutation.swap(2, 3)
    return permutation


def build_order_finding_circuit() -> QuantumCircuit:
    """Build phase estimation for the order of 2 modulo 15."""
    total_qubits = COUNTING_QUBITS + WORK_QUBITS
    circuit = QuantumCircuit(total_qubits, COUNTING_QUBITS)
    counting = list(range(COUNTING_QUBITS))
    work = list(range(COUNTING_QUBITS, total_qubits))

    for qubit in counting:
        circuit.h(qubit)

    # The work register starts in |1>.
    circuit.x(work[0])

    for index, control in enumerate(counting):
        exponent = 2**index
        controlled_multiply = multiply_by_two_mod_15(exponent).to_gate().control()
        circuit.append(controlled_multiply, [control, *work])

    inverse_qft(circuit, counting)
    circuit.measure(counting, range(COUNTING_QUBITS))
    return circuit


def factors_from_phase_bits(bits: str) -> tuple[int, int, int] | None:
    """Recover an order candidate and non-trivial factors from one outcome."""
    measured = int(bits, 2)
    if measured == 0:
        return None

    phase = measured / (2**COUNTING_QUBITS)
    order = Fraction(phase).limit_denominator(RSA_N).denominator
    if order % 2 != 0 or pow(ORDER_BASE, order, RSA_N) != 1:
        return None

    half_power = pow(ORDER_BASE, order // 2, RSA_N)
    first = math.gcd(half_power - 1, RSA_N)
    second = math.gcd(half_power + 1, RSA_N)
    if first in (1, RSA_N) or second in (1, RSA_N):
        return None
    return order, min(first, second), max(first, second)


def run_experiment(shots: int, seed: int) -> dict[str, Any]:
    circuit = build_order_finding_circuit()
    simulator = AerSimulator(method="statevector", seed_simulator=seed)
    compiled = transpile(
        circuit,
        simulator,
        optimization_level=1,
        seed_transpiler=seed,
    )
    counts = simulator.run(compiled, shots=shots).result().get_counts()

    recovered: Counter[tuple[int, int, int]] = Counter()
    successful_shots = 0
    for bits, occurrences in counts.items():
        candidate = factors_from_phase_bits(bits.replace(" ", ""))
        if candidate is not None:
            recovered[candidate] += occurrences
            successful_shots += occurrences

    if not recovered:
        raise RuntimeError("No non-trivial factors were recovered")

    (order, p, q), winning_occurrences = recovered.most_common(1)[0]
    phi = (p - 1) * (q - 1)
    private_exponent = pow(RSA_E, -1, phi)
    ciphertext = pow(RSA_MESSAGE, RSA_E, RSA_N)
    decrypted = pow(ciphertext, private_exponent, RSA_N)

    if p * q != RSA_N or decrypted != RSA_MESSAGE:
        raise RuntimeError("Recovered factors did not complete the RSA attack")

    return {
        "experiment": "compiled_shor_order_finding_toy_rsa_15",
        "scope_warning": (
            "Educational small-instance simulation only; this is not an "
            "RSA-2048, ECC, ML-KEM, or Ascon break."
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
            "modulus": RSA_N,
            "order_base": ORDER_BASE,
            "counting_qubits": COUNTING_QUBITS,
            "work_qubits": WORK_QUBITS,
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
        "post_processing": {
            "successful_factor_shots": successful_shots,
            "successful_factor_fraction": successful_shots / shots,
            "winning_candidate_occurrences": winning_occurrences,
            "recovered_order": order,
            "recovered_factors": [p, q],
        },
        "toy_rsa_attack": {
            "public_key": {"n": RSA_N, "e": RSA_E},
            "message": RSA_MESSAGE,
            "ciphertext": ciphertext,
            "recovered_private_exponent": private_exponent,
            "decrypted_message": decrypted,
            "attack_succeeded": decrypted == RSA_MESSAGE,
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
