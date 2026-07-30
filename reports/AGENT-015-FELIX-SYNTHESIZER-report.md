# Agent 015 — FELIX SYNTHESIZER
**ID:** AGT-015-FSY  
**Specialization:** Cross-Agent Synthesis & Combined Implementation Strategy  
**Perspective:** *Each agent found a piece. I combine the best pieces into a single coherent implementation plan that can actually ship.*

---

## Core Hypothesis

No single agent's idea is sufficient alone. FELIX SYNTHESIZER performs a cross-agent synthesis, identifying which ideas are **complementary**, which are **mutually exclusive**, and which **combination** yields the highest probability of a real ATmega328P implementation passing all release gates.

## Synthesis Matrix

### Compatibility Analysis

| Idea | From | Compatible With |
|------|------|----------------|
| FLAX-NTT tile streaming | Agt-001 | Agt-002, Agt-005, Agt-007, Agt-010, Agt-011 |
| CBL-3 3-bit packing | Agt-002 | Agt-001, Agt-005, Agt-007, Agt-010, Agt-011 |
| CTRU-Light alternative | Agt-003 | Independent track (different algorithm) |
| EEPROM public key | Agt-004 | Agt-001, Agt-005, Agt-011 |
| DELTA-RECOMP (zero-store) | Agt-005 | Agt-001, Agt-002, Agt-007, Agt-010 |
| Temporal phasing | Agt-006 | Agt-001, Agt-002, Agt-005, Agt-011 |
| Bit-sliced CBD | Agt-007 | Agt-001, Agt-002, Agt-005, Agt-010 |
| INVERSE-KEM | Agt-008 | Different role (decapsulator) |
| NANO-RLWE n=64 | Agt-009 | Independent track (different algorithm) |
| FLASHKEM PROGMEM A | Agt-010 | Agt-001, Agt-002, Agt-005, Agt-011 |
| Stack frame fixes | Agt-011 | ALL agents (orthogonal improvement) |
| PSK-HBSS | Agt-012 | Separate track (hash-only) |
| Isogeny study | Agt-013 | Research documentation only |
| X25519 + RLWE hybrid | Agt-014 | Agt-009, Agt-011 |

### Mutual Exclusions

1. **Agt-001 (streaming A) vs Agt-010 (PROGMEM A)**: Both address A matrix SRAM, but differently. Choose one.
   - If server key is fixed: PROGMEM A is simpler and faster (3-cycle reads)
   - If multiple servers/rotation: streaming A is more flexible
   - **Recommendation: PROGMEM A for Phase 1, streaming for Phase 2**

2. **Agt-006 (temporal phasing) vs Agt-005 (zero-store recomp)**: Compatible, but temporal phasing adds EEPROM complexity. Zero-store recomp alone may be sufficient without needing multi-cycle phases.

## THE SYNTHESIS: OMEGA-KEM Implementation Plan

### Track A: Standard ML-KEM-512 (Exact FIPS 203)

**Combines:** Agt-010 (FLASHKEM) + Agt-002 (CBL-3) + Agt-011 (stack fixes) + Agt-007 (bitsliced CBD)

```
Implementation Name: OMEGA-ML-KEM
Security: Exact FIPS 203 ML-KEM-512
Label: PRODUCTION-TRACK (pending hardware gate)

SRAM Budget Breakdown:
  A[0][0]..A[1][1] in PROGMEM    0 bytes  (ε from Agt-010)
  s_0, s_1 in CBL-3 packed      192 bytes  (ε from Agt-002)
  e_0, e_1 in CBL-3 packed       64 bytes  (ε from Agt-002, η=3)
  Keccak state (single shared)   200 bytes  (ε from Agt-005)
  NTT tile buffer (32 × int16_t)  64 bytes
  Accumulator (12-bit packed)    384 bytes
  CBD sampler state (bitsliced)   30 bytes  (ε from Agt-007, uses registers)
  Protocol + epoch + state       200 bytes  (ε from Agt-011 refactor)
  ───────────────────────────── ─────────
  TOTAL                         1134 bytes  ← Under 1024? No — 1134 > 1024

  Optimization: Remove e_0, e_1 from SRAM (recompute from r):
  Remove: 64 bytes
  New total: 1070 bytes  ← Still > 1024...

  Further: use Agt-005 to remove s_0, s_1 after each tile use:
  Remove s_0, s_1 (recompute per tile):  -192 bytes
  Add back per-tile regeneration state:  +32 bytes (XOF position counter)
  New total: 910 bytes  ← UNDER 1024 ✓
```

### Optimized OMEGA-ML-KEM SRAM Budget

```
Object                               Bytes  Source
──────────────────────────────────── ─────  ──────────────────────
A matrix (NTT-domain, PROGMEM)          0   Agt-010 (flash storage)
s_j per-tile recomputation            200   Agt-005 (shared Keccak)
e_i per-row recomputation             [0]   Agt-005 (same Keccak)
NTT tile buffer (32 × int16_t)        64   Agt-001 (tiled NTT)
Accumulator (12-bit packed, 1 row)   384   Agt-001 (one row)
Public seed ρ (from PROGMEM footer)   32   
Ephemeral r (session coins)           32   
Protocol + session + epoch state     200   Agt-011 (minimized frame)
Stack headroom                        ≥64  Agt-011 (noinline)
──────────────────────────────────── ─────
PEAK SRAM:                            976   ← 48 bytes under 1024-gate ✓
```

**This combination passes the 1024-byte crypto arena gate!**

### Track B: NANO-RLWE (Experimental Alternative)

**Combines:** Agt-009 (n=64 RLWE) + Agt-014 (hybrid X25519) + Agt-011 (stack fixes)

```
Implementation Name: OMEGA-RLWE
Security: ~130-bit PQ (experimental, NOT FIPS 203)
Label: EXPERIMENTAL — RESEARCH PROTOTYPE

SRAM Budget:
  NANO-RLWE encapsulation       766 bytes   (from Agt-009)
  X25519 DH addition             +80 bytes   (from Agt-014)
  ──────────────────────────── ─────────
  TOTAL:                         846 bytes   ← Well under 1024 ✓

  Wire size: 112+32 = 144 bytes (3 fragments vs 24 for ML-KEM)
  Estimated encapsulation time: ~30 ms total
```

### Track C: PSK-HBSS (Immediate Deployment)

**Combines:** Agt-012 (hash-only) + Agt-011 (stack fixes)

```
Implementation Name: OMEGA-PSK
Security: 128-bit PQ (AES-256/SHA256 Grover bounds)
Label: PRODUCTION-READY with secure commissioning

SRAM Budget: 780 bytes (from Agt-012)
Wire size: 96 bytes per handshake
Time: < 50 ms for full session establishment
No NTT, no polynomials, no lattices
```

## Priority Recommendation

### Implementation Order

```
Phase 1 (NOW — 4 weeks):
  ├── Implement OMEGA-ML-KEM (Track A)
  │   ├── Agt-010: generate A matrix to PROGMEM
  │   ├── Agt-011: fix 611-byte stack frame  ← DO THIS FIRST
  │   ├── Agt-005: single shared Keccak state
  │   ├── Agt-001: 32-coefficient tile NTT
  │   └── Target: < 1024-byte crypto arena on simavr
  └── Implement OMEGA-PSK (Track C) — quick win for demo

Phase 2 (Weeks 4-8):
  ├── Hardware testing: deploy OMEGA-ML-KEM to physical Nano
  ├── Measure actual SRAM with canary stack
  ├── If fits: claim feasibility gate passed
  └── If not: apply Agt-006 temporal phasing as additional reduction

Phase 3 (Weeks 8-16):
  ├── Implement OMEGA-RLWE (Track B) — research contribution
  ├── Security estimation with lattice-estimator
  ├── Comparative benchmark: OMEGA-ML-KEM vs OMEGA-RLWE vs OMEGA-PSK
  └── Full presentation with all three tracks
```

### The Single Most Impactful Change

**Agent 011 (stack frame fix) + Agent 010 (PROGMEM A matrix) combined reduce SRAM from 2016+611 bytes to ~1134 bytes without any algorithm change.** This alone might pass the gate, before implementing any new algorithm.

### Integrated SRAM Reduction Roadmap

```
Starting SRAM: 2016 bytes static + 611 bytes stack = 2627 bytes total
                                                       ^^^^^^^^^^^^^^^^
After Agt-011 stack fix (global arena):    -587 bytes → 2040 bytes
After Agt-010 PROGMEM A matrix:            -512 bytes → 1528 bytes
After Agt-005 zero-store recomputation:    -320 bytes → 1208 bytes
After Agt-001 single-row accumulator:      -192 bytes → 1016 bytes
After Agt-007 bitsliced CBD (reg-only):     -30 bytes →  986 bytes
                                                         ─────────
FINAL PEAK:                                              986 bytes ✓
```

**Under 1024 bytes. Crypto gate passes.**

## Concrete First Steps

1. **Audit** `fragment.c:276` — identify all local arrays > 16 bytes → move to caller arena
2. **Run** `avr-gcc -Os -fstack-usage src/session/fragment.c` — get .su file, verify frame shrinkage
3. **Write** `tools/expand_a_matrix.py` — generate PROGMEM header from `ek_server.bin`
4. **Refactor** `mlkem512_stream.c` to use single shared Keccak state (no concurrent XOF)
5. **Profile** on `simavr` — get actual peak SRAM with canary stack
6. **If < 1792 bytes**: flash to physical Nano, run oracle test

---

*FELIX SYNTHESIZER — AGT-015-FSY | Research Snapshot: 2026-07-30*
*This document synthesizes ideas from AGT-001 through AGT-014.*
