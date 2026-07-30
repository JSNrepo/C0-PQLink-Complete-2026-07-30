# Release evidence — 0.1.0

Run date: 2026-07-30  
Host compiler: GCC 13.3.0  
Node.js: 24.14.0  
Python: 3.12.13

## Passed

`make verify` completed with warnings enabled:

- C unit suite: passed;
- SHA3-256 empty vector: passed;
- SHA-256, RFC 4231 HMAC-SHA-256, and RFC 5869 HKDF: passed;
- Ascon-AEAD128 known-answer, decrypt, forged-tag rejection, and plaintext
  zeroization: passed;
- ML-KEM-512: 8/8 independent deterministic ciphertext/shared-secret
  comparisons passed;
- non-canonical ML-KEM-512 public key: rejected;
- maximum primitive ciphertext writer call: five bytes in all oracle cases;
- ratchet-mode C client ↔ JavaScript peer: passed;
- `FULL_PQ_EACH_SESSION` C client ↔ JavaScript peer: passed;
- injected loss: Challenge, fragment-7 ACK, Peer Finished, and first
  protected-data response recovered;
- injected tamper: fragment, Finished, and protected record rejected;
- forged duplicate Finished: rejected rather than served a cached response;
- exact data-request retry: cached response returned without duplicate
  application processing;
- oversized application records: rejected without consuming sequence state;
- maximum 48-byte application record: sealed into the 96-byte frame bound;
- replay and ratchet transaction tests: passed;
- all-zero PSK and public-key-ID placeholders: rejected;
- migration first-provisioning, no reinitialization, storage-I/O fail-closed,
  corrupt-record refusal, one-way upgrade, downgrade rejection, torn-write
  detection, and prior-slot recovery: passed;
- Arduino C++ wrapper and fail-closed sketch: host C++11 compile/link passed;
- public and Arduino header mirrors: identical;
- AddressSanitizer and UndefinedBehaviorSanitizer suite: passed;
- MicroPython facade: Python syntax compilation passed;
- reference peer/test scripts: Node syntax checks passed;
- demo provisioning generator: paired header/JSON output passed.
- UDP reference peer: configuration parse, bind, and readiness smoke test passed.
- Qiskit 2.5.1 / Aer 0.17.2 toy Shor and Grover runs: raw seeded
  counts reproduced;
- Qiskit transpiler levels 0–3 and Grover iterations 0–5: circuit metrics and
  raw counts recorded.

## Structural measurements

```text
mlkem_workspace_internal=1312
mlkem_workspace_public_bound=1344
client_context_this_abi=256
frame_max=96
record_plaintext_max=48
```

`client_context_this_abi` is the 64-bit host ABI value and must not be quoted
as the AVR value.

## Named ATmega328P rejection evidence

The current source was linked with AVR GCC 7.3.0 for `atmega328p`:

```text
Program: 20118 bytes (61.4%)
Data:     2016 bytes (98.4%) before stack
connect() compiler stack frame: 611 bytes
```

The current implementation is therefore **not runnable on the Nano**. The map,
ELF, and `.su` files are retained under `build/avr-nano-current/`; build output
is evidence, not a release artifact.

The object-disassembly audit finds no conditional instruction in the
signed-product correction function. This is a partial gate only; the linked
helper call graph and physical leakage remain pending.

## Explicitly skipped or unavailable

- CMake configure/install/consumer gate: CMake unavailable;
- native MicroPython named-port firmware build: source tree/toolchain
  unavailable;
- hardware stack high-water, timing, cycles, power, energy, radio airtime,
  RNG, reboot, fault, and leakage measurements: no target hardware;
- parser fuzzing, formal protocol analysis, and independent audit: not
  completed.

A skipped gate is not a pass. See `docs/CLAIM-LEDGER.md` for allowed wording.
