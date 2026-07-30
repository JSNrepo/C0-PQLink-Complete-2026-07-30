#!/bin/bash
# Deploy 15 Jules agents in parallel for hyper-parallel agentic research
# Each agent implements their specific idea from ideas/

REPO="JSNrepo/C0-PQLink-Complete-2026-07-30"
IDEA_DIR="ideas"

echo "=== DEPLOYING 15 JULES AGENTS ==="

# ============================================================
# AGENT 001 — ZETA STREAMWEAVER (AGT-001-ZSW)
# Specialization: NTT Pipeline Architecture & Flash-Mapped Coefficient Streaming
# ============================================================
cat << 'EOF' | jules new --repo "$REPO" "Agent 001: ZETA STREAMWEAVER - Implement FLAX-NTT"
You are AGENT-001-ZSW (ZETA STREAMWEAVER). Your research is in ideas/AGENT-001-ZETA-STREAMWEAVER.md.

YOUR TASK: Implement the FLAX-NTT (Flash-Loaded Accumulated eXecution NTT) algorithm for ATmega328P.

IMPLEMENT:
1. src/core/flax_ntt.c — tiled 32-coefficient NTT kernel with sliding-window operation
2. src/core/shake_seek.c — XOF seek/restart utilities for SHAKE128 state repositioning
3. src/core/mlkem512_flaxntt.c — full encapsulation using FLAX-NTT
4. include/c0pqlink/flax_ntt.h — public header

CONSTRAINTS:
- Peak SRAM must be < 1024 bytes (crypto arena)
- Use PROGMEM for twiddle factor table (256 bytes)
- Shared Keccak state (200 bytes) — seekable by design, no duplicate states
- Coefficients never simultaneously resident — streaming only
- Must pass oracle test: match ML-KEM-512 test vectors 8/8 exactly
- AVR-optimized Keccak-f (~2,800 cycles @ 16 MHz)
- Register only: s_j regenerated from coins, not stored full form

TARGET: 766-byte peak SRAM encapsulation with 180ms estimated time at 16 MHz.
Do NOT modify existing functional code paths. Add new files only.
EOF

echo "[JULES] Agent 001 submitted"

# ============================================================
# AGENT 002 — MIRA PACKWRIGHT (AGT-002-MPW)
# Specialization: Ternary Polynomial Compression & Coefficients Bit-Level Packing
# ============================================================
cat << 'EOF' | jules new --repo "$REPO" "Agent 002: MIRA PACKWRIGHT - Implement CBL-3 packing"
You are AGENT-002-MPW (MIRA PACKWRIGHT). Your research is in ideas/AGENT-002-MIRA-PACKWRIGHT.md.

YOUR TASK: Implement CBL-3 (Coefficient Bit-Level 3-bit Packing) for ML-KEM-512 polynomials on ATmega328P.

IMPLEMENT:
1. src/core/cbl3_pack.c — 3-bit coefficient pack/unpack for 256-coefficient polynomials
2. src/core/cbl3_arith.c — arithmetic operations directly on packed coefficients (NTT multiply, add, compress)
3. include/c0pqlink/cbl3.h — public header

CORE TECHNIQUE:
- 256 coefficients × 3 bits = 96 bytes per polynomial (vs 512 bytes for int16_t)
- Pack 2 coefficients per byte: byte[n] = (coeff[2n] << 3) | coeff[2n+1]
- Coefficients in range {0..7} for ternary (η=2) or CBD distributions
- Implement packed NTT: operate on 3-bit coefficients with Barrett reduction
- Implement packed add/poly_tomsg/poly_frommsg

SRAM TARGET:
- s_0 + s_1: 192 bytes (vs 1024 bytes baseline) — 5.3× reduction
- e_0 + e_1: 128-192 bytes (η=2 or η=3)
- All polynomial operations work on packed representation

Verify: unpack → NTT → multiply → pack should be bit-exact with reference.
Do NOT modify existing functional code paths. Add new files only.
EOF

echo "[JULES] Agent 002 submitted"

# ============================================================
# AGENT 003 — HECTOR SPLITCORE (AGT-003-HSC)
# Specialization: Alternative Ring/Lattice Design & Hardware-Aligned Parameter Selection
# ============================================================
cat << 'EOF' | jules new --repo "$REPO" "Agent 003: HECTOR SPLITCORE - Implement CTRU-Light"
You are AGENT-003-HSC (HECTOR SPLITCORE). Your research is in ideas/AGENT-003-HECTOR-SPLITCORE.md.

YOUR TASK: Implement CTRU-Light — a custom NTRU-based KEM optimized for ATmega328P.

IMPLEMENT:
1. src/core/ctru_light_core.c — CTRU-Light key generation, encapsulation, decapsulation
2. src/core/ctru_light_poly.c — truncated polynomial ring arithmetic (R_q = Z_q[X]/(X^n + 1))
3. include/c0pqlink/ctru_light.h — public header

PARAMETERS: n=128, q=2048, small ring with X^n+1 structure favoring bitwise operations

KEY ADVANTAGES FOR AVR:
- No NTT required — convolution via simple product scanning (no twiddle factors)
- q=2048 = 2^11 — modulus operations via bitmask (no Barrett/ Montgomery needed)
- n=128 — half the polynomial size of ML-KEM-512 (n=256)
- Polynomial = 128 × 11-bit coeffs = 176 bytes (vs 512 bytes for n=256 int16_t)

SRAM BUDGET:
- Secret polynomial f (ternary): 32 bytes (2-bit packed, 128 × 2 bits)
- Public key h: 176 bytes
- Polynomial workspace: 176 bytes
- Keccak state: 200 bytes
- Protocol state: 200 bytes
- TOTAL: ~784 bytes — well under 1024-byte crypto gate

Must pass deterministic test vector validation.
Do NOT modify existing functional code paths. Add new files only.
EOF

echo "[JULES] Agent 003 submitted"

# ============================================================
# AGENT 004 — NOVA HASHBRIDGE (AGT-004-NHB)
# Specialization: EEPROM-Integrated Key Caching & Hybrid Memory Architecture
# ============================================================
cat << 'EOF' | jules new --repo "$REPO" "Agent 004: NOVA HASHBRIDGE - EEPROM key caching"
You are AGENT-004-NHB (NOVA HASHBRIDGE). Your research is in ideas/AGENT-004-NOVA-HASHBRIDGE.md.

YOUR TASK: Implement EEPROM-Integrated Key Caching for ML-KEM-512 on ATmega328P.

IMPLEMENT:
1. src/eeprom/ek_cache.c — EEPROM-resident remote public key cache
2. src/eeprom/eeprom_helpers.c — EEPROM read/write with wear-leveling for 1024-byte EEPROM
3. include/c0pqlink/eeprom_cache.h — public header
4. Update src/session/fragment.c to load ek from EEPROM

EEPROM LAYOUT:
- Address 0-511: Encapsulation key ek (800 bytes? compress to 512)
- Address 512-543: Session nonce counter
- Address 544-575: Device fingerprint / metadata
- Address 576-1023: Reserved for future use

TECHNIQUE:
- Compress ek: store only seed ρ (32 bytes), regenerate A matrix at each session
- Full ek_regen() = XOF(seed_rho) → A matrix → compute t = A*s + e
- This trades 768 bytes SRAM for ~200ms computation at session start
- EEPROM wear: write only during key rotation (yearly), not per-session

TARGET: Reduce static SRAM by 400+ bytes by moving cached keys to EEPROM.
Do NOT modify existing functional code paths. Add new files only.
EOF

echo "[JULES] Agent 004 submitted"

# ============================================================
# AGENT 005 — AXEL RECOMPUTE (AGT-005-ARC)
# Specialization: Deterministic Recomputation & Zero-Storage Redundancy
# ============================================================
cat << 'EOF' | jules new --repo "$REPO" "Agent 005: AXEL RECOMPUTE - Zero-store recomputation"
You are AGENT-005-ARC (AXEL RECOMPUTE). Your research is in ideas/AGENT-005-AXEL-RECOMPUTE.md.

YOUR TASK: Implement DELTA-RECOMP — deterministic recomputation strategy that stores NOTHING in SRAM, regenerating every intermediate value from the 32-byte session seed r.

IMPLEMENT:
1. src/core/delta_recomp.c — recompute engine: given session coins r, regenerate s, e, r1 on demand
2. include/c0pqlink/delta_recomp.h — public header
3. Integration with src/core/mlkem512_stream.c

MECHANICS:
- Session coins r = 32 bytes (stored once, never leaves SRAM)
- Generate s = CBD_eta(CBD_seed(r, 0)) — call when s is needed for current tile
- Generate e1 = CBD_eta(CBD_seed(r, 1)) — call when e1 is needed
- Generate e2 = CBD_eta(CBD_seed(r, 2)) — call when e2 is needed
- Generate r1 = CBD_seed(r, 3) → compressed v → call when v is needed
- After use: DOES NOT STORE — next tile regenerates from same seed with position counter

SRAM SAVINGS:
- s: 0 bytes (recomputed per tile, was 256-512 bytes)
- e1: 0 bytes (recomputed per row, was 512 bytes)
- e2: 0 bytes (recomputed when needed)
- Cost: 200-byte Keccak state + 32-byte coins
- SAVINGS: ~1024-1280 bytes peak SRAM

TRADE-OFF:
- Each regeneration: ~8 SHAKE-128 squeezes (~800 cycles @ 16 MHz)
- Total cost: ~30 regenerations × 800 cycles = ~24,000 extra cycles = ~1.5ms
- Acceptable given >40× SRAM savings

Do NOT modify existing functional code paths. Add new files only.
EOF

echo "[JULES] Agent 005 submitted"

# ============================================================
# AGENT 006 — PRIYA MULTIPHASE (AGT-006-PMP)
# Specialization: Time-Sliced Execution & Phase-Multiplexed Memory
# ============================================================
cat << 'EOF' | jules new --repo "$REPO" "Agent 006: PRIYA MULTIPHASE - Temporal phase multiplexing"
You are AGENT-006-PMP (PRIYA MULTIPHASE). Your research is in ideas/AGENT-006-PRIYA-MULTIPHASE.md.

YOUR TASK: Implement temporal phase multiplexing — split the ML-KEM-512 encapsulation into 3 non-overlapping phases that share the same SRAM region at different times.

IMPLEMENT:
1. src/session/phase_kem.c — 3-phase encapsulation with tight SRAM reuse
2. include/c0pqlink/phase_kem.h — public header
3. EEPROM checkpoint/restore logic for multi-cycle encapsulation

PHASE DESIGN:
- Phase A (0-300ms): Generate u = NTT^{-1}(A^T × r) + e1
  - Uses: 384-byte accumulator, 200-byte XOF, 64-byte tile buffer
  - On complete: checkpoint 320-byte u compressed to EEPROM
  
- Phase B (300-600ms): Generate v = Compress(Decompress(u) × t + e2, dv)
  - Uses: same 384-byte accumulator (recycled), 200-byte XOF
  - On complete: checkpoint 96-byte v
  
- Phase C (600-800ms): Generate K = H(session_context) + verify
  - Uses: same 200-byte XOF, 256-byte hash state (recycled)
  - Result: 32-byte shared secret K

SRAM (per phase, never simultaneous):
- Phase A peak: 648 bytes
- Phase B peak: 584 bytes  
- Phase C peak: 456 bytes
- OVERALL PEAK: 648 bytes — substantially below 1024-byte gate!

EEPROM is used for phase checkpointing, not for SRAM extension.
Target: 800ms total encapsulation (acceptable for sensor IoT).
Do NOT modify existing functional code paths. Add new files only.
EOF

echo "[JULES] Agent 006 submitted"

# ============================================================
# AGENT 007 — KASPAR BITSLICE (AGT-007-KBS)
# Specialization: Bit-Sliced CBD & Register-Only Sampling
# ============================================================
cat << 'EOF' | jules new --repo "$REPO" "Agent 007: KASPAR BITSLICE - Bit-sliced CBD sampler"
You are AGENT-007-KBS (KASPAR BITSLICE). Your research is in ideas/AGENT-007-KASPAR-BITSLICE.md.

YOUR TASK: Implement a bit-sliced Centered Binomial Distribution (CBD) sampler that operates entirely in AVR registers (no SRAM for temporary polynomial storage).

IMPLEMENT:
1. src/core/cbd_bitslice.S — AVR assembly CBD sampler (η=2, η=3)
2. src/core/cbd_bitslice_c.c — C fallback with register hints
3. include/c0pqlink/cbd_bitslice.h — public header

TECHNIQUE:
- Process 8 coefficients per iteration using AVR's 8×8-bit register file
- CBD formula: a = popcount(seed[0:η]), b = popcount(seed[η:2η]), coeff = a - b
- Bit-slice: process 8 independent CBD samplings in one register batch
- Input: 8 bytes from SHAKE-128 output → 6 bytes of ternary coefficients (η=2)
- Output: coefficients written directly to NTT tile buffer or accumulator

REGISTER ALLOCATION (η=2):
- r18-r25: 8 seed bytes (one batch of CBD inputs)
- r26-r27: X pointer (output accumulator)
- r28-r29: Y pointer (seed position)
- r30-r31: Z pointer (twiddle table for NTT)
- Output: 4 bytes representing 8 × 2-bit packed coefficients

PERFORMANCE:
- Register-only CBD: ~40 cycles per 8 coefficients → ~1,280 cycles per full polynomial
- vs SRAM-based: ~2,500 cycles per polynomial + 256 bytes SRAM temporaries
- Speedup: ~2× faster, 256 bytes SRAM saved

Must be bit-exact with CBD_eta reference implementation.
Do NOT modify existing functional code paths. Add new files only.
EOF

echo "[JULES] Agent 007 submitted"

# ============================================================
# AGENT 008 — ELENA WIREDGRAPH (AGT-008-EWG)
# Specialization: Inverse Protocol Design & Decapsulator-Optimized Architecture
# ============================================================
cat << 'EOF' | jules new --repo "$REPO" "Agent 008: ELENA WIREDGRAPH - INVERSE-KEM decapsulation"
You are AGENT-008-EWG (ELENA WIREDGRAPH). Your research is in ideas/AGENT-008-ELENA-WIREDGRAPH.md.

YOUR TASK: Implement INVERSE-KEM — where the ATmega328P Nano acts as the DECAPSULATOR (receiving role, not sending).

IMPLEMENT:
1. src/core/inverse_kem.c — ML-KEM-512 decapsulation with fragment processing
2. src/session/fragment_process.c — receive ciphertext fragments and process immediately
3. include/c0pqlink/inverse_kem.h — public header
4. Update session protocol: Nano receives KEM ciphertext instead of generating it

KEY DESIGN:
- Nano stores dk_nano (NTT-domain secret key, 512 bytes) in EEPROM
- At session start: load dk_nano from EEPROM to SRAM (512 bytes)
- Receive ciphertext in 32-byte authenticated fragments via LoRa/802.15.4
- Process each u-row immediately upon receipt → never store full 768-byte ciphertext
- Decompress one u-row → NTT multiply-accumulate with sk_hat row → discard

SRAM PROFILE:
- sk_hat (NTT-domain, two rows): 512 bytes (permanent during session)
- One u-row working buffer: 320 bytes
- Keccak state: 200 bytes
- Protocol + session: 200 bytes
- TOTAL: 1232 bytes — fits under 1792-byte gate, borderline for 1024

ADVANTAGE: Nano does LESS computation (no keygen, no encapsulation), only decapsulation.
Do NOT modify existing functional code paths. Add new files only.
EOF

echo "[JULES] Agent 008 submitted"

# ============================================================
# AGENT 009 — THEO LOWDIM (AGT-009-TLD)
# Specialization: Reduced-Dimension Lattice Cryptography & Custom Parameter Design
# ============================================================
cat << 'EOF' | jules new --repo "$REPO" "Agent 009: THEO LOWDIM - NANO-RLWE n=64 KEM"
You are AGENT-009-TLD (THEO LOWDIM). Your research is in ideas/AGENT-009-THEO-LOWDIM.md.

YOUR TASK: Implement NANO-RLWE — a custom ring-LWE KEM with n=64 optimized for ATmega328P.

IMPLEMENT:
1. src/core/nanorldwe_core.c — n=64 Ring-LWE polynomial arithmetic (NTT, multiply, add)
2. src/core/nanorldwe_kem.c — full KEM (KeyGen, Encaps, Decaps) with n=64, q=769, σ=4
3. include/c0pqlink/nanorldwe.h — public header
4. python/tools/nanorldwe_reference.py — Python reference for test vector generation
5. tests/test_nanorldwe_oracle.c — oracle test with 8 deterministic vectors

PARAMETERS:
- n = 64 (ring dimension, 4× smaller than ML-KEM-512)
- q = 769 (prime, q ≡ 1 mod 128, enabling NTT for n=64)
- σ = 4.0 (error bound for ~130-bit quantum security)
- η = 1 (CBD: coefficients in {-1, 0, 1})
- k = 1 (single ring, not module)

KEY SIZES:
- Public key: 112 bytes (64 × 10 bits + 32 byte seed)
- Ciphertext: 112 bytes (u compressed + v)
- Secret key: 144 bytes (NTT-domain + pk + hash)

SRAM TARGET: 766 bytes peak for encapsulation
NTT: 6 layers, 192 butterfly ops (vs 1024 for n=256)
Label clearly: EXPERIMENTAL — NOT NIST STANDARDIZED — RESEARCH PROTOTYPE
Do NOT modify existing functional code paths. Add new files only.
EOF

echo "[JULES] Agent 009 submitted"

# ============================================================
# AGENT 010 — SIGMA PROGMEMIUS (AGT-010-SPM)
# Specialization: Flash Memory Architecture & PROGMEM-Resident Computation
# ============================================================
cat << 'EOF' | jules new --repo "$REPO" "Agent 010: SIGMA PROGMEMIUS - FLASHKEM PROGMEM matrix"
You are AGENT-010-SPM (SIGMA PROGMEMIUS). Your research is in ideas/AGENT-010-SIGMA-PROGMEMIUS.md.

YOUR TASK: Implement FLASHKEM — precompute the ML-KEM-512 public matrix A at compile time and store in PROGMEM flash.

IMPLEMENT:
1. tools/expand_a_matrix.py — Python script that takes ek_server.bin, expands A matrix via SHAKE128, outputs C header with PROGMEM qualifier
2. src/gen/ (directory) — generated A matrix header (run expand_a_matrix.py during build)
3. src/core/flashkem_mult.c — NTT multiplication using PROGMEM-resident A matrix tiles
4. include/c0pqlink/flashkem.h — public header
5. Update Makefile with pre-generation step

DESIGN:
- A[0][0], A[0][1], A[1][0], A[1][1] = 4 × 256 int16_t in PROGMEM = 2048 bytes flash
- Access via pgm_read_word_near() — 3 cycles per int16_t
- NTT-domain A matrix eliminates on-the-fly XOF expansion

SRAM IMPACT:
- A matrix: 512 bytes → 0 bytes in SRAM (moved to PROGMEM)
- Free 512 bytes of the most constrained resource

BUILD PIPELINE:
- python3 tools/expand_a_matrix.py --ek ek_server.bin --output src/gen/a_matrix_progmem.h
- avr-gcc includes the generated header
- Key rotation: rebuild firmware with new ek_server.bin

FLASH USAGE: 2048 bytes PROGMEM (6.4% of 32KB) — acceptable.
Do NOT modify existing functional code paths. Add new files only.
EOF

echo "[JULES] Agent 010 submitted"

# ============================================================
# AGENT 011 — REMY COMPACTOR (AGT-011-RCO)
# Specialization: Stack Frame Minimization & Compiler-Guided Memory Layout
# ============================================================
cat << 'EOF' | jules new --repo "$REPO" "Agent 011: REMY COMPACTOR - Stack frame minimization"
You are AGENT-011-RCO (REMY COMPACTOR). Your research is in ideas/AGENT-011-REMY-COMPACTOR.md.

YOUR TASK: Fix the 611-byte stack frame in c0pq_client_connect() and all other deep frames.

IMPLEMENT:
1. include/c0pqlink/c0pq_arena.h — arena structs for caller-allocated buffers
2. Refactor src/session/fragment.c — move all local arrays > 16 bytes to arena parameters
3. Refactor src/session/preflight.c — same treatment
4. Refactor src/core/mlkem512_stream.c — use arena for workspace
5. src/core/global_arena.c — single 672-byte global crypto arena in .bss
6. Add __attribute__((noinline)) to each phase function

TECHNIQUES:
- All large buffers → caller-provided arena struct (lifetime controlled by caller)
- Global arena in .bss (not stack!) → zero stack cost for 672-byte workspace
- __attribute__((noinline)) on phase functions → each phase has small independent frame
- Linker overlay section for crypto_workspace / ascon_state overlap

TARGET: 
- c0pq_client_connect frame: 611 bytes → < 32 bytes
- All function frames: < 64 bytes each
- Peak stack usage: < 128 bytes total

VERIFY: avr-gcc -fstack-usage on recompiled code — check .su files.
Do NOT modify algorithm logic — only memory layout and function signatures.
EOF

echo "[JULES] Agent 011 submitted"

# ============================================================
# AGENT 012 — DIANA HASHONLY (AGT-012-DHO)
# Specialization: Hash-Based Post-Quantum Signatures & Hybrid KEM-PSK
# ============================================================
cat << 'EOF' | jules new --repo "$REPO" "Agent 012: DIANA HASHONLY - PSK-HBSS protocol"
You are AGENT-012-DHO (DIANA HASHONLY). Your research is in ideas/AGENT-012-DIANA-HASHONLY.md.

YOUR TASK: Implement PSK-HBSS (Pre-Shared Key + Hash-Based Signature) protocol for ATmega328P.

IMPLEMENT:
1. src/session/psk_session.c — PSK-based session establishment with HKDF-SHA256 key derivation
2. src/core/hkdf_sha256.c — HKDF extract-and-expand using SHA256
3. src/core/xmss_verify_nano.c — XMSS server signature verification (Nano receives, does not sign)
4. include/c0pqlink/psk_session.h — public header
5. tests/test_psk_session.c — full session establishment test

PROTOCOL:
1. Nano: HELLO { device_id, epoch, nonce_nano (32 bytes) }
2. Server: CHALLENGE { nonce_server, server_timestamp, XMSS signature }
3. Nano: verify XMSS sig using 64-byte server pub key from EEPROM
4. Both: K_session = HKDF-SHA256(PSK, nonce_nano || nonce_server, device_id)
5. Ascon-AEAD128 traffic with K_session

SRAM BUDGET: 780 bytes
- SHA256 state: 96 bytes
- HMAC buffer: 64 bytes  
- HKDF inputs: 80 bytes
- Ascon state: 208 bytes
- Session key: 32 bytes
- Protocol state: 100 bytes
- XMSS verification: 200 bytes

SECURITY: 128-bit post-quantum (AES-256 Grover bounds + SHA256 collision resistance)
Do NOT modify existing functional code paths. Add new files only.
EOF

echo "[JULES] Agent 012 submitted"

# ============================================================
# AGENT 013 — OMAR ISOGENY (AGT-013-OIS)
# Specialization: Isogeny-Based Cryptography & Compact Key Protocols
# ============================================================
cat << 'EOF' | jules new --repo "$REPO" "Agent 013: OMAR ISOGENY - Isogeny feasibility study"
You are AGENT-013-OIS (OMAR ISOGENY). Your research is in ideas/AGENT-013-OMAR-ISOGENY.md.

YOUR TASK: Research and implement a feasibility prototype of SQIsign verification on ATmega328P.

IMPLEMENT:
1. src/core/sqifsign_verify.c — SQIsign verification for ATmega328P (receive-only, no signing)
2. src/core/gfpx_avr.c — GF(p²) arithmetic optimized for AVR (p ≈ 2^256)
3. include/c0pqlink/sqifsign.h — public header
4. docs/ISOGENY-FEASIBILITY.md — detailed timing analysis with measured clock cycles

DESIGN:
- Nano only VERIFIES SQIsign signatures — signing done on server
- Server pk = 64 bytes (stored in EEPROM)
- Signature = 177 bytes (received via fragments)
- GF(p²) multiplication via 16-bit MUL instructions (64×64-bit product → 128-bit result)

KEY CHALLENGE:
- 256-bit modular multiplication on 8-bit AVR: ~5000 cycles
- Full signature verification: ~15-30 minutes at 16 MHz
- Research path: Identify which sub-operations can be precomputed

ALTERNATIVE: SPI cryptographic co-processor (ATECC608B) integration:
- src/core/atecc608b_spi.c — SPI driver for ATECC608B
- Delegate isogeny operations to co-processor
- Nano SRAM: only 32-byte SPI buffer + 150-byte protocol state = 182 bytes

Label clearly: RESEARCH — NOT PRODUCTION READY
Do NOT modify existing functional code paths. Add new files only.
EOF

echo "[JULES] Agent 013 submitted"

# ============================================================
# AGENT 014 — LUNA HYBRIDCRAFT (AGT-014-LHC)
# Specialization: Hybrid Classical-PQ Protocol Design & Layered Security
# ============================================================
cat << 'EOF' | jules new --repo "$REPO" "Agent 014: LUNA HYBRIDCRAFT - X25519 + RLWE hybrid"
You are AGENT-014-LHC (LUNA HYBRIDCRAFT). Your research is in ideas/AGENT-014-LUNA-HYBRIDCRAFT.md.

YOUR TASK: Implement NANO-HYBRID — X25519 classical DH + NANO-RLWE post-quantum KEM hybrid.

IMPLEMENT:
1. src/core/x25519_avr.c — X25519 scalar multiplication for AVR (port from TweetNaCl, ~400 bytes code)
2. src/session/hybrid_kem.c — combined X25519 + NANO-RLWE session establishment
3. src/core/hybrid_combiner.c — HKDF combiner: K = HKDF(DH_secret || RLWE_secret, nonces)
4. include/c0pqlink/hybrid_kem.h — public header

PROTOCOL:
1. Nano: ek_x (X25519 ephemeral, 32 bytes) + nonce_nano → HELLO
2. Server: ek_peer_x + ek_peer_rlwe (112 bytes) → CHALLENGE
3. Nano: X25519(dk_x, ek_peer_x) = DH_classical
4. Nano: RLWE_Encaps(ek_peer_rlwe) = (K_pq, c) — 112-byte ciphertext
5. Nano: K_session = HKDF(DH_classical || K_pq, nonces)
6. Nano sends: c (112 bytes) → message exchange

SRAM BUDGET: 832 bytes
- X25519 keypair: 64 bytes
- X25519 scalar multiply: 80 bytes
- NANO-RLWE encapsulation: 360 bytes
- HKDF state: 96 bytes
- DH output: 32 bytes
- Protocol state: 200 bytes

SECURITY: Hybrid — attacker must break BOTH X25519 AND RLWE to compromise session.
No NTT on Nano (NANO-RLWE uses n=64 small ring without full NTT).
Do NOT modify existing functional code paths. Add new files only.
EOF

echo "[JULES] Agent 014 submitted"

# ============================================================
# AGENT 015 — FELIX SYNTHESIZER (AGT-015-FSY)
# Specialization: Cross-Agent Synthesis & Combined Implementation Strategy
# ============================================================
cat << 'EOF' | jules new --repo "$REPO" "Agent 015: FELIX SYNTHESIZER - Cross-agent synthesis"
You are AGENT-015-FSY (FELIX SYNTHESIZER). Your research is in ideas/AGENT-015-FELIX-SYNTHESIZER.md.

YOUR TASK: Synthesize the best ideas from Agents 001-014 into a single coherent implementation plan and implement the OMEGA-KEM integration layer.

IMPLEMENT:
1. src/core/omega_mlkem.c — OMEGA-ML-KEM (Track A): PROGMEM A + CBL-3 + tiled NTT + single Keccak
2. src/core/omega_rlwe.c — OMEGA-RLWE (Track B): n=64 RLWE + optional X25519 hybrid
3. src/session/omega_psk.c — OMEGA-PSK (Track C): hash-only PSK session
4. include/c0pqlink/omega.h — unified header with build-time selection
5. tests/test_omega_comparison.c — comparative benchmark of all three tracks
6. docs/OMEGA-INTEGRATION.md — integration guide

TRACK A (OMEGA-ML-KEM) — FIPS 203 compliant:
- A matrix: PROGMEM (Agt-010) — 0 bytes SRAM
- s, e: CBL-3 packed + recomputed per tile (Agt-002 + Agt-005) — 192 bytes
- Single shared Keccak state (Agt-005) — 200 bytes
- 32-coeff tile NTT (Agt-001) — 64 bytes
- Accumulator (12-bit packed, 1 row, Agt-001) — 384 bytes
- Arena-based stack (Agt-011) — 0 bytes stack overhead
- TOTAL SRAM: 910-976 bytes ← UNDER 1024 ✓

TRACK B (OMEGA-RLWE): ~846 bytes, ~130-bit PQ, experimental
TRACK C (OMEGA-PSK): ~780 bytes, 128-bit PQ, production-ready with commissioning

MAKE OMEGA phase1 the DEFAULT build target.
Do NOT modify existing functional code paths. Add new files only.
EOF

echo "[JULES] Agent 015 submitted"

echo ""
echo "=== ALL 15 AGENTS DEPLOYED ==="
echo "Check status: jules remote list --session"
