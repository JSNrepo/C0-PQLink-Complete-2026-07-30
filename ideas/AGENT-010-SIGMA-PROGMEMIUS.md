# Agent 010 — SIGMA PROGMEMIUS
**ID:** AGT-010-SPM  
**Specialization:** Flash Memory Architecture & PROGMEM-Resident Computation  
**Perspective:** *The ATmega328P has 32 KB of FLASH. That is 16× more than SRAM. Use it.*

---

## Core Hypothesis

The ATmega328P's 32 KB flash is currently used for code only. SIGMA PROGMEMIUS proposes **precomputing the entire public matrix A at compile time** and storing it in PROGMEM. This turns a 512-byte SRAM requirement (for the A matrix) into 512 bytes of flash, freeing the most expensive SRAM region entirely.

## Primary Proposal: FLASHKEM — PROGMEM-Resident Public Matrix

### What Can Live in PROGMEM?

The ML-KEM public matrix `A = XOF(ρ)` is:
- **Deterministic** from the 32-byte seed ρ
- **Public** (no secrecy needed — it's part of the public key)
- **Static** for a given device/key pair
- **Read-only** during encapsulation

These are exactly the properties required for `PROGMEM` storage in AVR.

### Pre-Computation Strategy

**At deployment time** (not at runtime):
```python
# Offline script (runs on host machine)
rho = bytes.fromhex("...")  # 32-byte seed from the server's public key

# Expand A matrix using SHAKE128
A = [[None, None], [None, None]]
for i in range(2):
    for j in range(2):
        A[i][j] = sample_ntt_poly(SHAKE128, rho + bytes([i, j]))

# Write A as C source file with PROGMEM qualifier
generate_progmem_header(A, "src/gen/a_matrix_progmem.h")
```

Output (`src/gen/a_matrix_progmem.h`):
```c
// AUTO-GENERATED — DO NOT EDIT
// Public matrix A for ML-KEM-512, seed ρ = 0x...
// Server public key fingerprint: SHA256(ek) = 0x...
#include <avr/pgmspace.h>

// A[0][0]: 256 int16_t values in NTT domain
const int16_t PROGMEM A_00[256] = {
    1234, 567, 8901, 2345, 6789, ...  // 256 values
};
// A[0][1], A[1][0], A[1][1] similarly
```

**Flash usage:** 4 polynomials × 256 × 2 bytes = 2048 bytes PROGMEM
**Current flash usage:** 20118 bytes (61.4%). After adding A: 22166 bytes (67.6%).
**Flash remaining:** ~10 KB — sufficient.

### SRAM Budget with FLASHKEM

```
Object                         Bytes  Notes
───────────────────────────── ─────  ────────────────────────────────────
Keccak state (for s, e, hash)   200  No A-expansion needed
Secret poly s_0 (3-bit packed)   96  CBL-3 from Agent 002
Secret poly s_1 (3-bit packed)   96  CBL-3
Error e_0, e_1 (3-bit packed)   192  CBL-3 (η=3)
Accumulator u_0 (12-bit packed) 384  One row at a time
NTT tile (working)               64  32 × int16_t
Protocol + session context      200
───────────────────────────── ─────
TOTAL                          1232  ← Fits 1792-byte gate!
```

**Flash A access is via `pgm_read_word_near()` — 3 cycles per int16_t (vs 2 for SRAM), but completely acceptable.**

### The PROGMEM NTT Accumulation Loop

```c
// A[i][j] is in NTT-domain PROGMEM, s[j] is in time-domain packed SRAM
// We need: accumulator += NTT(s[j]) * A[i][j]

// Strategy: generate NTT(s[j]) tile by tile, read matching A[i][j] tile
for (uint8_t tile = 0; tile < 8; tile++) {
    // Extract 32 coefficients of s[j] from 3-bit pack
    cbl3_extract_tile(s_j_packed, tile, tile_s);  // → int16_t[32]
    
    // Read 32 NTT-domain values of A[i][j] from PROGMEM
    for (uint8_t k = 0; k < 32; k++) {
        tile_A[k] = (int16_t)pgm_read_word_near(&A_ij[tile*32 + k]);
    }
    
    // Compute NTT of tile_s (partial — 6-layer for n=256, stride-based)
    ntt_tile_forward(tile_s, tile, twiddle_factors_progmem);
    
    // Pointwise multiply in NTT domain + Barrett reduction
    for (uint8_t k = 0; k < 32; k++) {
        accumulator_add(tile*32 + k, barrett_mul(tile_s[k], tile_A[k]));
    }
}
```

### Device-Specific Key Binding

This approach requires **per-device firmware** (since A matrix depends on the server's public key). Options:

**Option A: Shared A matrix** — all devices connect to the same server with the same `ek_server`, so A is identical. Single firmware binary. **Recommended for IoT deployments.**

**Option B: Personalized firmware** — each device has its own `ek_server`, firmware is personalized at commissioning. More complex build pipeline, stronger isolation.

**Option C: A in EEPROM** — store A matrix in EEPROM (2048 bytes needed, but EEPROM is only 1024 bytes — **doesn't fit**). Rejected.

Option A is the practical choice for sensor networks with a shared gateway.

### Flash Layout with FLASHKEM

```
Flash Region          Size    Content
───────────────────── ──────  ─────────────────────────────
Code (.text)          18000   C0-PQLink implementation
Static data (.data)    2118   Strings, constants
A_00 PROGMEM           512   A[0][0] NTT polynomial
A_01 PROGMEM           512   A[0][1] NTT polynomial
A_10 PROGMEM           512   A[1][0] NTT polynomial
A_11 PROGMEM           512   A[1][1] NTT polynomial
Twiddle factors PROGMEM 256  NTT twiddle table
───────────────────── ──────
TOTAL                 22422   (68.4% of 32 KB) — FITS!
```

### Build Pipeline

```makefile
# Pre-generation step (Python, runs once per key rotation)
src/gen/a_matrix_progmem.h: tools/expand_a_matrix.py ek_server.bin
	python3 tools/expand_a_matrix.py --ek ek_server.bin \
	  --output src/gen/a_matrix_progmem.h

# Main build depends on generated header
build/c0pqlink_nano.elf: src/gen/a_matrix_progmem.h $(ALL_SOURCES)
	avr-gcc -Os -mmcu=atmega328p ...
```

### Key Rotation Procedure

When the server rotates its ML-KEM keypair:
1. Generate new `ek_server`
2. Run `expand_a_matrix.py` → new `a_matrix_progmem.h`
3. Rebuild firmware binary
4. OTA-update all devices (or USB re-flash)

This is acceptable for IoT deployments — key rotation is infrequent (annually).

### Comparison Summary

| | Baseline | FLASHKEM |
|--|---------|---------|
| A matrix in SRAM | 512 bytes | **0 bytes** |
| A matrix in flash | 0 bytes | **2048 bytes** |
| Flash usage | 20118 (61.4%) | **22166 (67.6%)** |
| SRAM peak | 2016+ bytes | **~1232 bytes** |
| Requires keyed firmware | No | **Yes** |

---

*SIGMA PROGMEMIUS — AGT-010-SPM | Research Snapshot: 2026-07-30*
