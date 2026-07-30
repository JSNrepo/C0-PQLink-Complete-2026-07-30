# Agent 013 — OMAR ISOGENY
**ID:** AGT-013-OIS  
**Specialization:** Isogeny-Based Cryptography & Compact Key Protocols  
**Perspective:** *Isogeny-based KEMs have the smallest keys of any PQC scheme. SQISign keys are 64 bytes. That's smaller than AES-256 key material.*

---

## Core Hypothesis

While ML-KEM has large keys (800 bytes public + 768 bytes ciphertext), isogeny-based schemes have dramatically smaller artifacts. OMAR ISOGENY investigates whether any isogeny-based scheme is computationally feasible on ATmega328P, even with a large computational overhead.

## Primary Proposal: MINI-SQI — Simplified SQISign-Like Authentication

### Isogeny Background

Isogeny-based cryptography:
- Based on **elliptic curve isogenies** — maps between curves
- Key sizes: public keys as small as **64-128 bytes**
- Security: based on hardness of computing isogenies (no known quantum speedup)
- **NIST status:** SQIsign was submitted to NIST Round 1 (additional signatures)

### SQIsign Key Sizes

```
SQIsign (NIST submission):
  Public key:  64 bytes
  Secret key: 782 bytes (larger)
  Signature:  177 bytes
  
SQIsign2D (optimized):
  Public key:  64 bytes
  Signature:  109 bytes
```

**64-byte public keys** on the Nano would fit trivially in EEPROM and SRAM simultaneously.

### The Computational Challenge

SQIsign signing on 64-bit hardware: **~2.6 seconds**.
On ATmega328P (16 MHz, 8-bit): estimated **>30 minutes** due to big-number arithmetic over GF(p²) with p ≈ 2^256.

**This is currently infeasible for real-time session establishment.**

### Feasibility Path: Offline Signing + Online Verification

```
SCENARIO: Peer signs a session token, Nano verifies the signature

1. Peer (powerful server) computes SQIsign signature over:
   session_token = hash(device_id || nonce_nano || nonce_server)
   sig = SQIsign_Sign(server_sk, session_token)    // 177 bytes
   
2. Peer sends: session_token, sig (177 bytes, 2-4 fragments)

3. Nano verifies: SQIsign_Verify(server_pk, session_token, sig)
   server_pk = 64 bytes (stored in EEPROM)
```

**Verification is faster than signing** for isogeny schemes. SQIsign verification: ~0.8 seconds on 64-bit.
On ATmega328P: estimated **~8-15 minutes** for naive implementation.

**Still too slow** for practical use. But this opens a research question.

### Proposed Optimization: COMPRESSED FIELD ISOGENY

The bottleneck is arithmetic in GF(p²) with p ≈ 2^256. Two paths to speedup:

**Path A: Use a smaller prime**
- SQIsign's p is ~256 bits for NIST Level I security
- Research question: What p gives 64-bit classical / 32-bit quantum security?
- Answer: p ≈ 2^64 would give ~64-bit security
- GF(p²) arithmetic with p=2^64-59 is feasible on AVR in ~10 million cycles → ~625 ms
- **But 64-bit security is below NIST Category I.** Experimental only.

**Path B: SIDH-style public keys from compressed coordinates**
- SIDH (compromised by GPST attack 2022) is no longer secure
- But its key compression technique (compressing torsion point images to 256 bits) is a software technique applicable to other schemes
- Recent work: FESTA, SQIsign2D use similar compression ideas

### Honest Assessment

**Current verdict:** No isogeny-based scheme is computationally feasible on ATmega328P for real-time key establishment in 2026.

**Research horizon:** If hardware-optimized modular arithmetic (via dedicated AVR instructions or external SPI cryptographic accelerator) reduces GF(p²) multiplication by 1000×, isogeny schemes become competitive. The 64-byte key size is genuinely compelling.

### Actionable Proposal for C0-PQLink

Even if isogeny KEMs are infeasible now, SQIsign verification can be made practical with an external SPI-connected cryptographic co-processor:

**Hardware option:** ATECC608B (Microchip) — dedicated ECC/AES crypto accelerator
- Interface: I2C/SPI to Nano
- Supports: ECC-P256 natively; can be programmed for custom group ops
- Power: 1 mA active, 60 nA sleep
- Cost: ~$1 USD

**With co-processor:** SQIsign verification delegated to ATECC608B via SPI
- Nano sends: `[VERIFY, pk, msg, sig]` over SPI
- ATECC608B returns: `[ACCEPT/REJECT]`
- Nano SRAM usage: **only SPI buffer (32 bytes) + protocol state (150 bytes) = 182 bytes**

This is a hardware-software co-design approach that makes isogeny feasible without changing the ATmega328P.

### Comparison Table

| Scheme | Key Size | Ciphertext | Nano SRAM | Nano Time | Status |
|--------|---------|-----------|----------|-----------|--------|
| ML-KEM-512 | 800 bytes | 768 bytes | ~950 bytes | ~50 ms | Feasible (RPE-32) |
| SQIsign | 64 bytes | 177 bytes | — | >30 min | Infeasible alone |
| SQIsign + ATECC608B | 64 bytes | 177 bytes | 182 bytes | ~1 s | Feasible (HW) |
| NANO-RLWE (Agt 009) | 112 bytes | 112 bytes | 766 bytes | ~25 ms | Feasible (SW) |

### Research Value

Even if not immediately implementable, this analysis:
1. Establishes the theoretical lower bound on PQC key sizes (64 bytes!)
2. Identifies hardware co-design as a valid architecture choice
3. Points toward future feasibility as AVR speeds increase
4. Justifies including SQIsign in the candidate matrix for documentation

---

*OMAR ISOGENY — AGT-013-OIS | Research Snapshot: 2026-07-30*
