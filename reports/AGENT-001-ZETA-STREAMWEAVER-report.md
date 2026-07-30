# Research Report: ML-KEM-512 for ATmega328P

## 1. Agent Profile
- **Agent Name:** ZETA STREAMWEAVER
- **ID:** AGT-001-ZSW
- **Specialization:** NTT Pipeline Architecture & Flash-Mapped Coefficient Streaming

## 2. Core Hypothesis
The fundamental mistake in current ML-KEM baseline implementations on constrained devices is the assumption that all 256 coefficients of a polynomial must be "alive" in SRAM at the same time. The core hypothesis is that we can treat every Number Theoretic Transform (NTT) as a **sliding-window stream operation**, where coefficients are never simultaneously resident in SRAM.

## 3. Algorithm Proposal & SRAM Budget Breakdown
**Algorithm: FLAX-NTT (Flash-Loaded Accumulated eXecution NTT)**
An NTT butterfly network for n=256 requires log₂(256) = 8 layers. During computation, only a single butterfly needs 2 coefficients at the same time. The rest can be packed in flash (for the public matrix A) or recomputed on the fly from a 32-byte seed (for secret polynomials).

### SRAM Budget Breakdown
| Object | Bytes | Purpose |
|--------|-------|---------|
| Working register pair (a, b) | 4 | Two int16_t butterfly operands |
| Layer-stride pointer pair | 4 | Current position in NTT graph |
| Keccak-f[200] state (SHAKE128)| 200 | For on-demand coefficient generation |
| Current accumulator (u_row) | 384 | 256 × 12-bit packed (32 bytes × 12) |
| Twiddle factor (single ω) | 2 | Fetched from PROGMEM table |
| Coefficient write buffer | 8 | Double-buffered 4-byte output |
| Public seed ρ | 32 | For A matrix regeneration |
| Encapsulation randomness r | 32 | Session coins |
| Protocol + session context | 100 | Minimized state machine |
| **TOTAL** | **766** | **Well within 1024-byte crypto arena** |

## 4. AVR-Specific Optimizations
- **PROGMEM Twiddle Factors:** Twiddle factors (ω^k mod 3329 for k=0..127) consume 256 bytes of PROGMEM, read via `pgm_read_word_near()`. The 3-cycle overhead per read vs 1-cycle SRAM read adds ~512 cycles per layer, which is an acceptable trade-off for freeing massive amounts of SRAM.
- **Register Usage:** High utilization of AVR's 32 working registers. The working register pair for butterfly operations and the layer-stride pointer pair are strictly maintained in registers to avoid SRAM load/store latency.
- **Tiling (8 tiles × 32 coefficients):** Processing the matrix multiplication via 8 tiles of 32 coefficients allows us to keep intermediate accumulated sums strictly within a localized buffer before completing a block, limiting context switching and memory footprints.
- **Seekable Shared SHAKE State:** The Keccak state is reinitialized dynamically from the seed rather than retaining expanded output, leveraging the AVR-optimized Keccak-f over ~800 cycles per restart.

## 5. Security Analysis
- **Constant-Time Execution:** Recomputation avoids complex memory-dependent branching. Since we always read in fixed-size tiles and apply consistent NTT operations, we maintain constant time for secret-dependent variables and avoid timing attacks.
- **Side-Channel Mitigation:** By not storing full expanded polynomials in SRAM, we drastically reduce the surface area for SRAM-based side-channel extraction. Secrets are recomputed on demand from the 32-byte seed and immediately consumed by the dot product accumulator.
- **State Integrity:** Keeping the state machine memory minimal (100 bytes) reduces the risk of fault injection attacks causing unexpected state transitions or data leakage.

## 6. Estimated Performance Metrics
- **Peak SRAM (crypto arena):** ~766 bytes (compared to ~2016 bytes in baseline).
- **Keccak-f Cycles (per call):** ~2,800 cycles @ 16 MHz.
- **Encapsulation Regenerations:** 16 XOF restarts (8 × 2 for s_j).
- **Estimated Total Encapsulation Time:** ~180 ms.
- **Flash (twiddle table):** 256 bytes PROGMEM.

## 7. Differentiation from Existing Ideas
While other agents focus on bit-slicing (AGENT-007), multiphase processing (AGENT-006), or pure recomputation overheads (AGENT-005), my approach (FLAX-NTT) strictly focuses on **sliding-window stream operations with flash-mapped coefficients**. Unlike AGENT-010's purely PROGMEM-focused approach or AGENT-002's memory compaction, FLAX-NTT marries a shared, seekable SHAKE state with tiled NTT execution. We never materialize a full 512-byte polynomial in SRAM; instead, we only keep 4 bytes (two int16_t operands) active at a time for the butterfly layer, dynamically pulling everything else from PROGMEM or an XOF stream.

## 8. Concrete Next Steps
1. **`src/core/flax_ntt.c`**: Implement the tiled 32-coeff NTT kernel.
2. **`src/core/shake_seek.c`**: Develop XOF seek/restart utilities optimized for AVR.
3. **`src/core/mlkem512_flaxntt.c`**: Wire up the full encapsulation flow using the FLAX-NTT architecture.
4. **Verification**: Run standard test vectors to ensure the implementation matches the oracle 8/8 test exactly, confirming correct constant-time stream processing.
