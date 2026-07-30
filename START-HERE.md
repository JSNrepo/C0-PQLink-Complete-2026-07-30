# Start here: complete C0-PQLink source package

This package contains the implementation, tests, simulation code, evidence, and
research documents. It is not a document-only handoff.

## Where the code is

| Path | Contents |
|---|---|
| `src/core/` | Exact ML-KEM-512 encapsulation, Keccak, SHA-256, and Ascon-AEAD128 C code |
| `src/session/` | Preflight, ciphertext fragmentation, Finished verification, record protection, ratchet, and migration logic |
| `include/c0pqlink/` | Public portable-C API |
| `src/C0PQLink.*` | Arduino/C++ wrapper |
| `examples/LiveSensorClient/` | Arduino library example and adapter points |
| `micropython/` | Native MicroPython user module and Python facade |
| `reference-peer/` | Replaceable JavaScript interoperability peer |
| `tests/` | C unit, oracle, cross-language session, fault-injection, and Arduino API tests |
| `quantum-tests/` | Executable Qiskit Shor, Grover, and circuit-optimization experiments |
| `tools/` | Host verification, AVR build, size, and disassembly checks |
| `evidence/` | Raw Qiskit results, candidate matrix, and captured AVR build evidence |
| `docs/` | Protocol, threat model, research verdict, scoring map, and claim ledger |

## Verify the host implementation

On Ubuntu or Kali Linux, install `build-essential`, Node.js, npm, and Python 3,
then run:

```sh
sh ./tools/verify-complete-package.sh
```

The script installs the locked JavaScript dependency when necessary, runs the
portable-C and cross-language tests, compiles the Arduino-facing API on the
host, runs the sanitizer gate, reports the memory objects, validates the
Python source, and checks the JSON evidence files.

## Reproduce the quantum simulations

Use the isolated virtual-environment script so Kali's system Python is not
modified:

```sh
sh ./tools/run-qiskit-evidence.sh
```

This installs the pinned Qiskit versions from `quantum-tests/requirements.txt`
and regenerates the JSON files under `evidence/qiskit/`.

## Current hardware status

The host implementation and simulator evidence are executable. The captured
ATmega328P link evidence is included under `evidence/avr-nano-current/`.

The current Nano image is **not a passing Nano release**: it uses 2,016 of
2,048 static SRAM bytes before runtime stack, interrupts, transport, or sensor
logic. The proposed RPE-32 recomputation schedule in
`docs/HYPER-RESEARCH-VERDICT.md` is a research design that has not yet been
implemented. Physical-Nano execution, measured energy, and whole-program
stack high-water evidence also remain pending.

Use `docs/CLAIM-LEDGER.md` as the authoritative boundary between executed
evidence, derived values, proposed work, and unverified claims.
