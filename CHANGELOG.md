# Changelog

## 0.1.0 — 2026-07-30

Initial research-preview package:

- exact streamed FIPS 203 ML-KEM-512 client encapsulation;
- callback-backed 800-byte public key;
- 1,344-byte bounded phase-overlaid workspace;
- PSK-authenticated Hello/Challenge preflight;
- 16 authenticated 48-byte ciphertext fragments with bounded retry;
- transcript-bound bilateral Finished;
- SP 800-232 Ascon-AEAD128 live records;
- optional transactional per-record symmetric ratchet;
- one-way migration states and authenticated A/B journal;
- Arduino/C++ wrapper and fail-closed example;
- native MicroPython user-module wrapper and frozen facade;
- replaceable independent JavaScript peer;
- unit, oracle, cross-language interop, loss, tamper, replay, journal, C++,
  and sanitizer gates.

Known pending gates are tracked in `docs/CLAIM-LEDGER.md`.

Post-release correction: the named ATmega328P link uses 2,016 bytes of static
SRAM before stack and is therefore not Nano-runnable. The research replacement
is tracked in `docs/HYPER-RESEARCH-VERDICT.md`.
