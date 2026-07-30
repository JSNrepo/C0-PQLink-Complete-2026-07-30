# Agent 005 — AXEL RECOMPUTE
**ID:** AGT-005-ARC  
**Specialization:** Deterministic Recomputation Schedules & Zero-Memory Secret Expansion  
**Perspective:** *SRAM is not memory. SRAM is a temporary staging area. Every byte that can be recomputed from a seed should be.*

---

## Core Hypothesis

The maximum possible SRAM reduction comes from a principle AXEL RECOMPUTE calls **ZERO-STORE**: any value derivable from a seed that fits in 32 bytes should **never be stored** — only its seed is kept. ML-KEM-512's two secret polynomials s_0, s_1 (512 bytes each) are fully deterministic from 64 bytes of coins. They need zero bytes of SRAM between uses.

## Algorithm Proposal: DELTA-RECOMP (Deterministic Expanded Lazy Timed Arithmetic — RECOMPuted)

### The Recomputation Inventory

In ML-KEM-512 encapsulation, given only `m` (32 bytes) and `ek` (800 bytes), everything is deterministic:

```
K̄  = G(m ∥ H(ek))               ← 64 bytes output, split: K' (32) + r (32)
(ρ) = stored in ek[768:800]       ← 32 bytes — public matrix seed
A   = XOF(ρ, i, j)               ← fully deterministic from ρ
s_i = SamplePolyCBD(PRF(r, i))   ← deterministic from r
e_i = SamplePolyCBD(PRF(r, k+i)) ← deterministic from r
```

Everything derives from two 32-byte seeds: `ρ` (public) and `r` (ephemeral private).

### DELTA-RECOMP Strategy

#### Phase A: Generation of r (32 bytes) — Store Permanently During Session
```c
uint8_t r[32];   // 32 bytes SRAM — kept for the full encapsulation
```

#### Phase B: On-Demand s_i Generation — Never Stored Full Form
```c
// To get coefficient j of polynomial s_i:
static int16_t get_s_coeff(uint8_t poly_idx, uint16_t coeff_idx) {
    // Restart PRF(r, poly_idx) each time — Keccak only needs 200 bytes
    uint8_t prf_input[33] = {poly_idx};
    memcpy(prf_input + 1, r, 32);
    
    // SHAKE256(prf_input, 33) → extract bytes at position coeff_idx*2
    // for CBD sampling (deterministic)
    shake256_init(&xof_state, prf_input, 33);
    shake256_squeeze_to_pos(&xof_state, coeff_idx * 2);  // seek forward
    // 2 bytes of XOF output → one CBD sample (2 bits from each byte)
    return sample_cbd_eta3(&xof_state);
}
```

**Cost:** 200-byte Keccak state + O(coeff_idx) squeeze operations.
**Optimization:** For sequential access (tile-by-tile), do NOT restart — use streaming.

### Full DELTA-RECOMP NTT Integration

```
for row i in {0,1}:
    clear_accumulator();   // 384-byte packed buffer — zero

    for col j in {0,1}:
        // Initialize PRF(r, j) in XOF state — 200 bytes SRAM
        xof_init_for_s(j);   
        
        for tile t in 0..7:  // 8 tiles × 32 coefficients = 256
            // Get 32 coefficients of s_j for this tile
            // (sequential: no restart, just continue squeezing)
            gen_s_tile(j, t, tile_s);  // fills 32×int16_t → 64 bytes tile
            
            // Get 32 coefficients of A[i][j] for this tile
            // (regenerated from ρ and position counters)
            gen_A_tile(i, j, t, tile_A);   // same 64-byte buffer reused
            
            // dot product: 32 multiplications mod 3329
            dot_accumulate(tile_s, tile_A, accumulator);
        
        // End of column j: s_j is gone, will be regenerated for e calc
    
    // Add error e_i: regenerate from PRF(r, 2+i)
    xof_init_for_e(i);
    add_error_streamed(i, accumulator);
    
    // Compress + stream 320 bytes of u_i out
    compress_stream_u_i(accumulator, output_callback);
```

### DELTA-RECOMP SRAM Budget

```
Object                          Bytes  Notes
─────────────────────────────── ─────  ───────────────────────────────
Keccak/SHAKE state              200    Shared for all polynomial ops
Tile buffer s_j (32 × int16_t)  64    Reused every tile
Tile buffer A_ij (32 × int16_t) 64    Same 64-byte region (reused)
Accumulator (12-bit packed)     384    384 bytes for one u-row
Seed r                           32    Session ephemeral
Seed ρ (from ek footer)          32    Public
K' (intermediate hash output)   32    Temporary
Protocol + state machine        150    Minimized
─────────────────────────────── ─────
TOTAL                           958    ← FITS in 1024-byte crypto gate!
```

### Trade-Off: Speed vs SRAM

| Approach | SRAM | Encapsulation Cycles |
|----------|------|---------------------|
| Standard ML-KEM | 2016 bytes | ~500K cycles |
| DELTA-RECOMP (naive restart) | **958 bytes** | ~3.2M cycles |
| DELTA-RECOMP (streaming) | **958 bytes** | ~800K cycles |

The streaming variant (continue squeezing sequentially) avoids restarts, adding only ~60% cycle overhead vs the standard approach — a completely acceptable trade-off.

**At 16 MHz: 800K cycles ≈ 50 ms** for encapsulation. Well within IoT session establishment budgets.

### The Shared Keccak State Pattern

The single most important insight: all of ML-KEM's polynomial generation uses SHAKE/PRF which ultimately is Keccak. With ONE 200-byte Keccak state, we handle:
- A matrix generation (via SHAKE128 over ρ)
- Secret polynomial generation (via PRF using r)
- Error polynomial generation (via PRF using r + offset)
- Hash computations (SHA3 uses same Keccak core)

This is valid because each XOF operation runs start-to-finish before the next begins. **The Keccak state is a single shared resource, not N concurrent ones.**

### AVR Implementation Notes

```c
// The state machine ensures only one XOF is "live" at any time
typedef enum {
    XOF_IDLE,
    XOF_RUNNING_A,     // generating A matrix coefficients
    XOF_RUNNING_S,     // generating secret polynomial
    XOF_RUNNING_E,     // generating error polynomial
    XOF_RUNNING_HASH   // SHA3/H() computation
} xof_state_tag_t;
```

No concurrent XOF states needed. Sequential single-use of 200-byte buffer.

### Verification Plan

1. Implement `delta_recomp_encaps()` in C
2. Run against the 8 oracle test vectors
3. All 768 ciphertext bytes must match byte-for-byte
4. Measure SRAM peak with AVR canary stack
5. Profile on `simavr` for cycle counts

---

*AXEL RECOMPUTE — AGT-005-ARC | Research Snapshot: 2026-07-30*
