# Constrained-cryptography research map

This note places C0-PQLink beside the publications raised during the project.
It deliberately separates standards, work in progress, and research
proposals.

The expanded candidate measurements, keep/correct/discard audit, RPE-32
execution hypothesis, Qiskit platform evidence, and release gates are in
[`HYPER-RESEARCH-VERDICT.md`](HYPER-RESEARCH-VERDICT.md).

## NIST SP 800-232 — Ascon

**Status:** final NIST Special Publication, August 2025.  
**What it supplies:** lightweight authenticated encryption, hash, and XOF
primitives for constrained environments.

**Useful here:** After ML-KEM establishes keying material, C0-PQLink uses
Ascon-AEAD128 for short live records. NIST specifies 128-bit security strength
for Ascon-AEAD128 in the single-key, nonce-respecting setting.

**What it does not supply:**

- public-key key establishment;
- peer discovery or identity;
- post-quantum digital signatures;
- nonce lifecycle, fleet provisioning, or a complete session protocol.

The main operational failure mode is nonce reuse under the same key.
C0-PQLink derives directional base nonces, uses exact monotonic sequence
numbers, and requires a fresh session after reboot rather than restoration of
a live key/sequence pair.

Reference:
[NIST SP 800-232](https://csrc.nist.gov/pubs/sp/800/232/final).

## NIST FIPS 203 — ML-KEM

**Status:** final FIPS, August 2024. C0-PQLink targets the published
ML-KEM-512 encoding and algorithms and checks its deterministic output against
an independent implementation.

The NIST publication page currently carries an errata planning note. A future
FIPS correction or revision must be reviewed explicitly; C0-PQLink must not
silently change suite `0x0001` wire behavior. If corrected normative bytes or
requirements differ, assign a new suite/version and add official vectors.

Reference:
[NIST FIPS 203](https://csrc.nist.gov/pubs/fips/203/final).

## IETF constrained-PQC guidance

**Status on 2026-07-30:** `draft-ietf-pquip-pqc-hsm-constrained-06`, an
Internet-Draft with intended status Informational. It is work in progress,
not yet an RFC or Internet Standard.

The draft covers seed/private-key storage, ephemeral key management, lazy
expansion, artifact sizes, performance benchmarking, key rotation, firmware
considerations, and side-channel protection.

### Seed-storage trade-off

Storing a seed instead of an expanded private key can reduce **persistent
storage**. It does not automatically remove peak RAM, CPU, latency, energy, or
side-channel costs when the key is expanded. Seeds require protection equal
to private keys.

C0-PQLink's constrained device is the ML-KEM **encapsulator**, so it does not
own or expand the peer's private key. Its relevant storage object is the
peer's public key. C0-PQLink therefore uses callback-backed flash reads and
recomputation rather than pretending a private-key seed solves the client
workspace problem.

### Offloading

The draft surveys offloading as one possible architecture. C0-PQLink does not
select that option: ML-KEM encapsulation remains on the client. The reference
peer performs only the standard private-key decapsulation role.

### Guidance adopted by C0-PQLink

- explicit key IDs and epochs;
- efficient ephemeral handling and immediate erasure;
- bounded memory with lazy/on-demand expansion ideas;
- measured rather than assumed target performance;
- key rotation and cryptographic agility;
- side-channel review as a separate mandatory gate.

Reference:
[IETF constrained-PQC Internet-Draft](https://datatracker.ietf.org/doc/draft-ietf-pquip-pqc-hsm-constrained/).

## NIST SP 800-208 — LMS/XMSS

**Status:** final NIST recommendation, October 2020.  
**What it supplies:** stateful hash-based digital signature schemes LMS/HSS
and XMSS/XMSS-MT.

**Why it is not in C0-PQLink:** This project is a live-connection KEM and
record layer, not a signature, secure-boot, or firmware-update system.
Adding SP 800-208 would increase scope without solving session key
establishment.

Stateful hash signatures require the signing system to prevent one-time-key
state reuse. Power loss, backup/restore, multi-signer coordination, and
nonvolatile wear are serious operational concerns. A constrained device that
only verifies a vendor signature has a different state burden from the
signing authority, so statements that every verifier must update signing
state are inaccurate.

SP 800-208 is a technical recommendation, not by itself a universal legal
mandate for every smart meter, vehicle, or industrial device.

Reference:
[NIST SP 800-208](https://csrc.nist.gov/pubs/sp/800/208/final).

## PQ-IoTCrypt / BRLWE research

**Status:** academic research proposal, not a NIST or IETF standard.

The reported design uses a Binary Ring-LWE-based construction and tailored
distributions/arithmetic for constrained IoT. Such work is useful for
research comparison, but a smaller benchmark is not sufficient evidence for
production cryptography. A new construction needs extensive public
cryptanalysis, precise security reductions and parameters, side-channel
analysis, interoperable specifications, independent implementations, and a
migration ecosystem.

C0-PQLink therefore does not adopt PQ-IoTCrypt or modify ML-KEM mathematics.
Its research contribution is implementation scheduling, memory overlay,
streaming, authenticated fragmentation, and migration around a standardized
KEM.

Reference:
[PQ-IoTCrypt article record](https://www.sciencedirect.com/science/article/abs/pii/S2542660524003329).

## Decision summary

| Option | Decision | Reason |
|---|---|---|
| Ascon-AEAD128 | Use | Final constrained-device AEAD standard for traffic |
| Exact ML-KEM-512 | Use | Standardized KEM and independently testable bytes |
| Seed-only peer private key on client | Not applicable | Client is encapsulator and owns no ML-KEM private key |
| Cryptographic offload | Reject | Violates device-local encapsulation goal |
| LMS/XMSS | Exclude | Signature/firmware scope, not live key establishment |
| PQ-IoTCrypt/modified BRLWE | Exclude | Non-standard new primitive and ecosystem risk |

This makes the novelty claim narrow and defensible: **standard primitives,
new constrained execution and transport composition, openly labeled as an
experimental protocol.**
