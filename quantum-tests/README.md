# Quantum attack evidence

This directory contains executable quantum demonstrations, not security proof
by failed simulation.

`qiskit_shor_rsa15.py` runs a real Qiskit Aer order-finding circuit compiled
for the deliberately tiny modulus 15. It recovers the factors, derives the toy
RSA private exponent, and decrypts a ciphertext. The JSON output preserves the
raw shot counts, seed, package versions, circuit size/depth, recovered order,
factors, and end-to-end attack result.

`qiskit_grover_toy_key.py` runs a real four-qubit Grover search over a
deliberately tiny 16-key space. It records the raw counts and the actual
probability of measuring the marked key. It illustrates generic quadratic
search only; it is not an attack on Ascon's 128-bit key or ML-KEM's 256-bit
shared secret.

`qiskit_optimization_study.py` makes the quantum-optimization claim
measurable. It transpiles the two circuits at optimization levels 0, 1, 2, and
3 into the fixed `rz/sx/x/cx` basis and records circuit depth, size,
one-/two-/multi-qubit operation counts, raw shots, and result probability. It
also sweeps zero through five Grover iterations, preserving every raw count
distribution. Wall-clock timings are recorded as environment-specific
observations; seeded counts and circuit metrics are the primary reproducible
evidence.

It must be described as:

> A compiled Shor order-finding simulation against toy RSA-15 that demonstrates
> why scalable quantum computers threaten factorization-based cryptography.

It must **not** be described as an attack on RSA-2048, ECC-256, ML-KEM-512, or
Ascon-AEAD128. A classical simulator cannot establish ML-KEM security by
failing to break it. C0-PQLink's ML-KEM evidence instead comes from exact
FIPS-203-compatible outputs, independent implementation interoperability,
malformed-input tests, target-device execution, and cited cryptanalysis.

Run in an isolated environment:

```sh
python3 -m venv .venv-qiskit
. .venv-qiskit/bin/activate
python -m pip install -r quantum-tests/requirements.txt
python quantum-tests/qiskit_shor_rsa15.py \
  --output evidence/qiskit/shor-rsa15.json
python quantum-tests/qiskit_grover_toy_key.py \
  --output evidence/qiskit/grover-toy-key.json
python quantum-tests/qiskit_optimization_study.py \
  --output evidence/qiskit/optimization-study.json
```
