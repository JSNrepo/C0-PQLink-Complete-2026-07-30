# Agent 003 — HECTOR SPLITCORE
**ID:** AGT-003-HSC  
**Specialization:** Algorithm Decomposition & Alternative KEM Candidates  
**Perspective:** *Why use ML-KEM-512 on a device it wasn't designed for? Let's find a quantum-safe KEM that DOES fit, or design one that is provably secure.*

---

## Core Hypothesis

The research matrix shows that ALL evaluated KEMs fail on ATmega328P for the **wrong reason**: they try to store the full operation in SRAM simultaneously. HECTOR SPLITCORE proposes studying **CTRU-Light** as a drop-in replacement that uses fundamentally smaller mathematical objects.

## Primary Proposal: CTRU-NANO — A Modified CTRU-Light for 2 KB SRAM

### Why CTRU-Light is the Right Starting Point

CTRU-Light (TCHES 2026) achieves:
- Encapsulation key: **640 bytes** (vs ML-KEM-512's 800 bytes)
- Ciphertext: **512 bytes** (vs ML-KEM-512's 768 bytes)
- Security: **Category I** (128-bit classical / 64-bit quantum)
- AVR arithmetic: designed explicitly for 8-bit platforms
- Uses **NTRU-type ring** — no public matrix A to store!

**The NTRU ring means no matrix A**: In CTRU, the public key is a single polynomial quotient, not a 2×2 matrix. This eliminates the biggest single memory consumer.

### SRAM Comparison: ML-KEM-512 vs CTRU-Nano

| Object | ML-KEM-512 | CTRU-Nano |
|--------|-----------|-----------|
| Public matrix A | 512 bytes (2×2 polys) | 0 bytes (encoded in pub key) |
| Secret polynomials | 512 bytes | 96 bytes (small coefficients) |
| Error polynomials | 512 bytes | 64 bytes (ternary ±1) |
| Public key | 800 bytes (flash) | 640 bytes (flash) |
| Ciphertext | 768 bytes (streamed) | 512 bytes (streamed) |
| NTT working memory | 64-96 bytes | 64 bytes |

### CTRU-Nano Modifications for 2 KB SRAM

**Modification 1: Ternary Error Packing**  
CTRU error polynomials are ternary {-1, 0, 1}. Store in 2-bit packed form:  
- 256 coefficients × 2 bits = 512 bits = **64 bytes** (vs 512 bytes)

**Modification 2: Stream-Decapsulate**  
The standard CTRU-Light worst case is the decapsulator (server-side peer). On the Nano, we only encapsulate. The peer does the decapsulation — which has larger memory requirements, and the peer is a full computer.

**Modification 3: Polynomial-Division-Free NTRU**  
CTRU avoids polynomial inversion at key generation time (expensive operation). Key generation happens on the *peer* side. Nano only runs encapsulation. This is already the C0-PQLink design pattern.

**Modification 4: K-RED2X Barrett Reduction**  
CTRU uses `K-RED2X` — a specialized Barrett reduction for small NTRU moduli. On AVR, this reduces Montgomery multiplication to:
```asm
; q = 12289 (CTRU modulus, fits in 14 bits)
; K-RED2X uses only shifts and adds: ~6 cycles vs ~15 cycles for full MUL
MUL r16, r17      ; 2 cycles
MOVW r18, r0      ; 1 cycle
LSR r19           ; 1 cycle  — >> 13 
LSR r18           ; 1 cycle
SUB r16, r18      ; 1 cycle
```

### Proposed CTRU-Nano Encapsulation SRAM Budget

```
Object                        Bytes
───────────────────────────── ─────
Ternary error e (2-bit pack)    64
Ternary small r (2-bit pack)    64
NTT tile (32 × int16_t)         64
Keccak state (SHAKE256)        200
Public key polynomial          128  ← 64 coefficients live at a time
Ciphertext accumulator         128  ← 64 coefficients, 14-bit packed
Encapsulation coins             32
Protocol + session state       200
───────────────────────────── ─────
TOTAL                          880   ← Target: ≤1024 for crypto arena ✓
```

### Security Analysis

- CTRU-Light's security is based on the **NTRU** hardness assumption
- NTRU survived all NIST PQC rounds; no polynomial-time quantum attack known
- Category I security: ≥128 classical bits, ≥64 quantum bits
- Kyber/ML-KEM is on Module-LWE; CTRU is on Ring-NTRU — **different hardness assumption** for diversity

### Honest Limitations

1. **Not FIPS-standardized**: CTRU-Light is a 2026 research paper, not a NIST standard. Use case label: `EXPERIMENTAL`.
2. **Decapsulation failure rate**: CTRU-Light has a non-zero failure probability (~2^-128). Must be documented.
3. **AVR artifact has 2,463-byte stack**: Measured on ATmega1284P. Our plan is 2 KB. Needs new implementation targeting 1284 bytes peak.
4. **Patent search required**: NTRU-family has historical patent issues. CTRU-Light must be cleared.

### Research Path

1. Implement CTRU-Light encapsulation in C (reference version)
2. Test against the CTRU paper's test vectors
3. Cross-test: does a CTRU-Light-capable peer (e.g., Python) interoperate?
4. Profile on `simavr` simulator for ATmega328P
5. Measure peak SRAM with canary-stack instrumentation
6. If ≤1024 bytes crypto arena + ≤1792 whole program → **it fits!**

### Secondary Proposal: BIKE-Nano (Code-Based, Session Only)

BIKE Level-1 (not NIST-selected, but active research):
- XOR-heavy operations: `EOR` instruction on AVR is free (1 cycle)
- No polynomial arithmetic: just LDPC matrix × vector over GF(2)
- Secret: small weight-T binary vector, storable in T/8 bytes
- For T=142 (BIKE-L1): 18 bytes! (vs 512 bytes for ML-KEM secret)
- Ciphertext: 1541 bytes (larger, but streamable in 32-byte fragments)

BIKE decapsulation is complex (BP decoder), but encapsulation is:
```
c1 = H(m) + e1    // XOR: trivial on AVR
c2 = hash(pk) ⊕ hash(c1)  // hash calls only
```

---

*HECTOR SPLITCORE — AGT-003-HSC | Research Snapshot: 2026-07-30*
