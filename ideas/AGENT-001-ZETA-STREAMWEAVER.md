# Agent 001 — ZETA STREAMWEAVER
**ID:** AGT-001-ZSW  
**Specialization:** NTT Pipeline Architecture & Flash-Mapped Coefficient Streaming  
**Perspective:** *Treat the ATmega328P as a stream processor with infinite virtual memory in flash.*

---

## Core Hypothesis

The fundamental mistake in ML-KEM baseline implementations is assuming all 256 coefficients of a polynomial must be "alive" in SRAM at the same time. ZETA-STREAMWEAVER proposes that we treat every NTT as a **sliding-window stream operation** where coefficients are never simultaneously resident.

## Algorithm Proposal: FLAX-NTT (Flash-Loaded Accumulated eXecution NTT)

### Key Insight
An NTT butterfly network for n=256 has log₂(256) = 8 layers. At any point during computation, only a **single butterfly** needs 2 coefficients simultaneously. The rest can be stored packed in flash (public matrix A) or recomputed from a 32-byte seed (secret polynomials).

### SRAM Budget Breakdown

```
Object                          Bytes
─────────────────────────────── ─────
Working register pair (a, b)       4   ← two int16_t butterfly operands
Layer-stride pointer pair          4   ← current position in NTT graph
Keccak-f[200] state (SHAKE128)   200   ← for on-demand coefficient gen
Current accumulator (u_row)      384   ← 256 × 12-bit packed (32 bytes × 12)
Twiddle factor (single ω)          2   ← fetched from 512-byte PROGMEM table
Coefficient write buffer           8   ← double-buffered 4-byte output
Public seed ρ                     32   ← for A matrix regeneration
Encapsulation randomness r        32   ← session coins
Protocol + session context       100   ← minimized state machine
─────────────────────────────── ─────
TOTAL                            766   ← Well within 1024-byte crypto arena
```

### PROGMEM Usage
- **Twiddle factors** (ω^k mod 3329 for k=0..127): 256 bytes of PROGMEM
- **Public matrix A**: Read progressively via callback; never in SRAM
- **NTT layer schedule**: 32-byte lookup of stride sizes

### Algorithm Steps (Encapsulation)

```c
// FLAX-NTT Encapsulation — pseudo-code
for each row i in {0,1}:
    clear_accumulator_384();          // zero 12-bit packed buffer
    for each col j in {0,1}:
        // Regenerate s_j from coins — never stored full form
        xof_seek(coins, j);           // 32 bytes to restart SHAKE
        for each tile t in 0..7:     // 8 tiles × 32 coefficients
            gen_secret_tile(t, buf_96); // generate 32×3-bit coeffs
            // Stream A[i][j] tile-aligned from flash callback
            read_A_tile(i, j, t, a_tile); 
            // Compute 32-element dot product, accumulate into 12-bit
            dot_accumulate(buf_96, a_tile, accumulator);
        // one inverse NTT pass over 384-byte accumulator (tiled)
    add_error_e1_i();                 // recompute e1_i from coins
    compress_and_stream_u_i();        // stream 320 bytes out
```

### Critical Insight: Shared SHAKE State

Instead of storing expanded polynomials, the SHAKE128 XOF state (200 bytes) is **seekable by design**. We reposition it using a byte counter and reinitialize from the 32-byte seed. This makes every ephemeral polynomial regeneratable in ~800 clock cycles using the AVR-optimized Keccak-f.

### Expected AVR Metrics (Estimated)

| Metric | Value |
|--------|-------|
| Peak SRAM (crypto arena) | ~766 bytes |
| Keccak-f cycles (per call) | ~2,800 cycles @ 16 MHz |
| Encapsulation regenerations of s_j | 8 × 2 = 16 XOF restarts |
| Estimated total encapsulation time | ~180 ms |
| Flash (twiddle table) | 256 bytes PROGMEM |

### Why This Is Better Than Current Baseline

| | Baseline | FLAX-NTT |
|--|---------|----------|
| Polynomial SRAM | 512 bytes | 0 bytes |
| Accumulator | 512 bytes | 384 bytes |
| XOF state | 200 bytes | 200 bytes (shared) |
| Error vectors | 512 bytes | 0 bytes (recomputed) |
| **SRAM total** | **~2016 bytes** | **~766 bytes** |

### Deliverable

Implementation path:
1. `src/core/flax_ntt.c` — the tiled 32-coeff NTT kernel
2. `src/core/shake_seek.c` — XOF seek/restart utilities  
3. `src/core/mlkem512_flaxntt.c` — full encapsulation using FLAX-NTT
4. Verification: must match oracle 8/8 test vectors exactly

### Open Gate
The twiddle factor table in PROGMEM requires `pgm_read_word_near()` — a 3-cycle overhead per coefficient vs 1-cycle SRAM read. Total cost: +512 cycles per full-layer butterfly pass. **Acceptable** given the SRAM savings.

---

*ZETA STREAMWEAVER — AGT-001-ZSW | Research Snapshot: 2026-07-30*
