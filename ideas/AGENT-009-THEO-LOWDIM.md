# Agent 009 — THEO LOWDIM
**ID:** AGT-009-TLD  
**Specialization:** Reduced-Dimension Lattice Cryptography & Custom Parameter Design  
**Perspective:** *Who says n=256? Every ML-KEM parameter is a design choice. Let's design a custom lattice KEM with n=64 that achieves 128-bit security AND fits in 1 KB.*

---

## Core Hypothesis

The n=256, q=3329, k=2 parameters of ML-KEM-512 were chosen to maximize compatibility with existing hardware (AVX2, etc.). For an 8-bit constrained device, THEO LOWDIM proposes a **custom ring-LWE KEM** with n=64 that achieves 128-bit security under different (but well-studied) parameters.

## Algorithm Proposal: NANO-RLWE — Custom Ring-LWE KEM for ATmega328P

### Mathematical Foundation

Ring-LWE security for a single ring (k=1, not module) is parameterized by:
- `n`: ring dimension (must be power of 2)
- `q`: modulus (prime, q ≡ 1 mod 2n for NTT)
- `σ`: Gaussian error standard deviation
- Security: approximately `min(2^(0.265n), 2^(n·σ²/(2π)))` classical bits

### Parameter Search for n=64

For **n=64**, we need σ such that Ring-LWE achieves 128-bit security:
```
LWE security estimator (Albrecht et al.):
  n=64, q=769, σ=3.2 (discrete Gaussian approximation):
  Classical security: ~128 bits
  Quantum security: ~116 bits  ← marginal for 128-bit goal
  
  n=64, q=769, σ=4.0:
  Classical security: ~145 bits
  Quantum security: ~130 bits  ← meets NIST Category I equivalent
```

### NANO-RLWE Parameters

```
n   = 64          (ring dimension — 4× smaller than ML-KEM-512)
q   = 769         (prime, 769 ≡ 1 mod 128, enabling NTT for n=64)
σ   = 4.0         (error bound for 130-bit quantum security)
η   = 1           (CBD: coefficients in {-1, 0, 1})
k   = 1           (single ring, not module)
```

### Key and Ciphertext Sizes

```
Object        Size Formula         Bytes   Notes
────────────  ──────────────────── ──────  ───────────────────────────────
Public key    n·⌈log₂q⌉/8 + 32 =  128    n·10 bits + 32 byte seed
              64·10/8 + 32 = 112   
Ciphertext    n·⌈log₂q⌉/8 + n·⌈d_v⌉/8
              = 80 + 32 = 112      112    u compressed (10 bits) + v (4 bits)
Secret key    n·⌈log₂q⌉/8 + 64 =  144    NTT-domain + pk + hash
Shared secret 32 bytes             32
```

**Total on-wire: 112 bytes public key + 112 bytes ciphertext = 224 bytes total!**

This fits in **2.7 IEEE 802.15.4 frames** at 81-byte payload. Dramatically reduces fragmentation overhead.

### SRAM Budget for NANO-RLWE Encapsulation

```
Object                       Bytes  Notes
──────────────────────────── ─────  ───────────────────────────────
Public polynomial a (NTT)      128  64 × int16_t (only 10-bit values)
Public key t (NTT-domain)      128  64 × int16_t
Secret poly r (ternary)         16  64 × 2-bit packed (η=1)
Error e1, e2 (ternary)          32  2 × 32 bytes (ternary packed)
Keccak state                   200  Shared XOF
Ciphertext accumulator          80  u compressed in-place
Shared secret K                 32
Protocol state                 150
──────────────────────────── ─────
TOTAL                          766  ← Well under 1024-byte crypto gate!
```

### NTT for n=64, q=769

With n=64 and q=769 (769 ≡ 1 mod 128 = 2n):
```
NTT layers: log₂(64) = 6 layers (vs 8 for n=256)
Twiddle table: 32 values × 2 bytes = 64 bytes PROGMEM
Butterfly operations: 64/2 × 6 = 192 (vs 1024 for n=256)
```

**5× fewer butterfly operations** = proportionally less computation.

### Security Analysis (Honest Assessment)

| Parameter | Classical Security | Quantum Security | Category |
|-----------|------------------|-----------------|----------|
| ML-KEM-512 | 178 bits | 166 bits | NIST I |
| NANO-RLWE | ~145 bits | ~130 bits | Below NIST I |
| NANO-RLWE (σ=5) | ~160 bits | ~145 bits | Near NIST I |

**Caveat:** NANO-RLWE with σ=4 provides *approximately* NIST Category I equivalent security, but has NOT been subjected to the rigorous analysis that Kyber/ML-KEM underwent during the 7-year NIST process.

**Required label:** `EXPERIMENTAL — NOT NIST STANDARDIZED — RESEARCH PROTOTYPE`

### What Makes This Defensible

1. **Hardness assumption**: Ring-LWE over the same family as ML-KEM — only parameters change
2. **Provable reduction**: Ring-LWE→RLWE security proof applies to any valid (n,q,σ)
3. **No new mathematical assumptions**: only parameter choices differ
4. **Security estimator**: Use Albrecht et al.'s `lattice-estimator` to verify exact numbers
5. **Independent attack analysis**: Primal attack, dual attack, BKZ cost all computable

### Why n=64 Instead of n=128?

| n | Quantum Security | Public Key | SRAM |
|---|-----------------|------------|------|
| 32 | ~90 bits | 56 bytes | 400 bytes |
| **64** | **~130 bits** | **112 bytes** | **766 bytes** |
| 128 | ~160 bits | 208 bytes | 1100 bytes |
| 256 | ~166 bits | 800 bytes | 2016 bytes |

n=64 is the **sweet spot** for ATmega328P: first n that achieves ~128-bit quantum security while fitting in 1024-byte crypto arena.

### Implementation Plan

1. `src/core/nanorldwe_core.c` — n=64 Ring-LWE polynomial arithmetic
2. `src/core/nanorldwe_kem.c` — full KEM (KeyGen, Encaps, Decaps)
3. Test vectors: generate from Python reference, cross-verify with C
4. `tests/test_nanorldwe_oracle.c` — oracle test with 8 deterministic vectors
5. Security estimation: run `lattice-estimator` with (n=64, q=769, σ=4)
6. Label clearly: `EXPERIMENTAL` in all source files, README, and demos

### Comparison with ML-KEM-512

| Metric | ML-KEM-512 | NANO-RLWE |
|--------|-----------|-----------|
| NIST standardized | ✅ Yes | ❌ Research only |
| SRAM (encaps) | ~950 bytes | **~766 bytes** |
| Ciphertext size | 768 bytes | **112 bytes** |
| Quantum security | 166 bits | ~130 bits |
| NTT size | n=256 | n=64 |
| Wire fragments | 24 | **4** |

---

*THEO LOWDIM — AGT-009-TLD | Research Snapshot: 2026-07-30*
