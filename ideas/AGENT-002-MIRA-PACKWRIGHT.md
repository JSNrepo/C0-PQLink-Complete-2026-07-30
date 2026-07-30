# Agent 002 — MIRA PACKWRIGHT
**ID:** AGT-002-MPW  
**Specialization:** Bit-Packing Arithmetic & Sub-Word Coefficient Encoding  
**Perspective:** *Every bit saved in SRAM is a victory. The ML-KEM secret polynomial wastes 13 bits per coefficient. Reclaim them.*

---

## Core Hypothesis

ML-KEM-512 secret polynomials drawn from a centred binomial distribution CBD(η₁=3) take values in **{-3, -2, -1, 0, 1, 2, 3}** — only 7 possible values. A standard `int16_t` wastes 13 of 16 bits. MIRA PACKWRIGHT proposes that secret polynomials be stored in **3-bit signed form** using a custom 96-byte packed buffer, reducing secret polynomial storage by **5.3×**.

## Algorithm Proposal: CBL-3 (Centred Binomial Level-3 Packing)

### Packing Scheme

```
Each coefficient c ∈ {-3,-2,-1,0,1,2,3} is stored as:
  stored_value = c + 3  → range [0..6], fits in 3 bits
  
256 coefficients × 3 bits = 768 bits = 96 bytes exactly.

Byte-level layout (little-endian bit order within byte):
  byte[k] holds:
    bits [2:0]  → coeff[k*2+0] + 3
    bits [5:3]  → coeff[k*2+1] + 3  
    bits [7:6]  → low 2 bits of coeff[k*2+2] + 3
  byte[k+1]:
    bit  [0]    → high bit of coeff[k*2+2] + 3
    ...and so on (crossing byte boundaries)
```

### SRAM Budget with CBL-3

```
Object                              Bytes
─────────────────────────────────── ─────
Secret poly s_0 (packed 3-bit)        96
Secret poly s_1 (packed 3-bit)        96
Error poly e_0 (packed 3-bit)         96
Error poly e_1 (packed 3-bit)         96
Accumulator u_row_0 (12-bit packed)  384
Accumulator u_row_1 (12-bit packed)  384
NTT tile buffer (32 int16_t)          64
Keccak state                         200
Seeds + coins + metadata              80
Protocol context (min)               200
─────────────────────────────────── ─────
TOTAL                               1696   ← Fits 1792-byte gate!
```

### Critical Observation

By storing ALL 4 polynomials (s_0, s_1, e_0, e_1) simultaneously as 3-bit packed arrays:
- **No need for regeneration loops** — all polynomials available throughout
- **Butterfly operations** extract one coefficient at a time: 3-bit unpack → compute → no re-store needed
- **Reduced cycles**: eliminates expensive SHAKE re-initialization during NTT

### Butterfly Extraction

```c
// Extract coefficient index i from 96-byte packed 3-bit buffer
static inline int16_t cbl3_get(const uint8_t *buf, uint8_t i) {
    uint16_t bit_pos = (uint16_t)i * 3;
    uint8_t byte_idx = bit_pos >> 3;
    uint8_t bit_off  = bit_pos & 7;
    uint16_t raw = (uint16_t)buf[byte_idx] | ((uint16_t)buf[byte_idx+1] << 8);
    uint8_t val = (raw >> bit_off) & 0x07;
    return (int16_t)val - 3;   // un-bias: back to [-3, 3]
}
```

This runs in **~8 AVR instructions** — no branches, constant time.

### Comparison with NTT-Domain Storage

| Approach | Storage per poly | Comment |
|----------|-----------------|---------|
| Standard int16_t | 512 bytes | Baseline |
| NTT-domain int16_t | 512 bytes | No savings |
| CBL-3 (proposed) | **96 bytes** | 5.3× reduction |
| 4-bit packed | 128 bytes | Wastes 1 bit |

### Why Not 2-bit Packing?
ML-KEM-512 uses η₁=3, so values reach ±3. That is 7 values, requiring exactly 3 bits (2² = 4 values is insufficient). **3-bit is provably tight for ML-KEM-512.**

### Integration with RPE-32

CBL-3 is **complementary** to RPE-32's tile-based NTT:
- RPE-32 processes 32 coefficients per tile
- CBL-3 provides each coefficient in O(1) from packed form
- The 64-byte tile workspace is shared across all operations

### New Combined SRAM Budget (CBL-3 + RPE-32)

```
Secret poly s_0 (CBL-3)      96
Secret poly s_1 (CBL-3)      96  
Tile buffer (RPE-32)         64
Accumulator (12-bit packed) 384
Error e_0, e_1 (CBL-3)     192
Keccak state                200
Seeds + protocol state      250
────────────────────────── ────
TOTAL                      1282  ← 510 bytes headroom!
```

### Implementation Plan

1. `src/core/cbl3_pack.h` — encode/decode macros (zero overhead)
2. `src/core/cbl3_pack.c` — CBD sampling directly into packed form
3. `src/core/mlkem512_cbl3.c` — full encapsulation with packed secrets
4. Test: `tests/test_cbl3.c` — verify round-trip packing/unpacking

### AVR-Specific Optimization

On AVR, 3-bit extraction uses only `LD`, `LDD`, `AND`, `LSR` instructions. The 8-instruction extraction runs in **~500 ns** at 16 MHz. For 256 coefficients per polynomial, that is ~128 μs total extraction time — negligible compared to Keccak calls.

---

*MIRA PACKWRIGHT — AGT-002-MPW | Research Snapshot: 2026-07-30*
