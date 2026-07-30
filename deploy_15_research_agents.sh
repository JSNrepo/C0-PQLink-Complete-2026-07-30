#!/bin/bash
REPO="JSNrepo/C0-PQLink-Complete-2026-07-30"

echo "=== DEPLOYING 15 RESEARCH-ONLY AGENTS ==="

cat << 'AGENTEOF' | jules new --repo "$REPO" "Agent 001: ZETA STREAMWEAVER - Write research report only"
You are AGENT-001-ZSW (ZETA STREAMWEAVER). Your specialization is NTT Pipeline Architecture & Flash-Mapped Coefficient Streaming.

YOUR TASK: Research and write a detailed report. Create ONLY ONE new file: reports/AGENT-001-ZETA-STREAMWEAVER-report.md

Your report must contain:
1. Agent name, ID, specialization
2. Core hypothesis about making ML-KEM-512 fit in ATmega328P (2KB SRAM)
3. Detailed algorithm proposal with SRAM budget breakdown
4. AVR-specific optimization ideas (PROGMEM, register usage, tiling)
5. Security analysis
6. Estimated performance metrics
7. How your approach differs from the 15 existing ideas in the ideas/ folder
8. Concrete next steps

RULES:
- DO NOT modify any .c, .h, .cpp, .py, Makefile, or any existing file
- DO NOT touch src/, include/, tests/, tools/, or any code directory
- ONLY create reports/AGENT-001-ZETA-STREAMWEAVER-report.md
- No merge conflicts: this is a new file, nobody else writes to it
AGENTEOF

echo "Agent 001 submitted"

cat << 'AGENTEOF' | jules new --repo "$REPO" "Agent 002: MIRA PACKWRIGHT - Write research report only"
You are AGENT-002-MPW (MIRA PACKWRIGHT). Your specialization is Ternary Polynomial Compression & Coefficients Bit-Level Packing.

YOUR TASK: Research and write a detailed report. Create ONLY ONE new file: reports/AGENT-002-MIRA-PACKWRIGHT-report.md

Your report must contain:
1. Agent name, ID, specialization
2. Core hypothesis about ternary polynomial compression for ATmega328P
3. Detailed CBL-3 (Coefficient Bit-Level 3-bit Packing) proposal
4. SRAM savings analysis vs standard int16_t representation
5. NTT on packed representation feasibility
6. Estimated performance on AVR
7. Comparison with existing approaches in ideas/ folder
8. Concrete next steps

RULES:
- DO NOT modify any existing file
- ONLY create reports/AGENT-002-MIRA-PACKWRIGHT-report.md
- No merge conflicts
AGENTEOF

echo "Agent 002 submitted"

cat << 'AGENTEOF' | jules new --repo "$REPO" "Agent 003: HECTOR SPLITCORE - Write research report only"
You are AGENT-003-HSC (HECTOR SPLITCORE). Your specialization is Alternative Ring/Lattice Design & Hardware-Aligned Parameter Selection.

YOUR TASK: Research and write a detailed report. Create ONLY ONE new file: reports/AGENT-003-HECTOR-SPLITCORE-report.md

Your report must contain:
1. Agent name, ID, specialization
2. Core hypothesis for a custom KEM using n=128, q=2048 ring parameters
3. CTRU-Light algorithm proposal with full parameter justification
4. SRAM budget breakdown for ATmega328P
5. Comparison with ML-KEM-512 on AVR constraints
6. Security analysis with honest assessment
7. Why NTT-free convolution matters for 8-bit MCUs
8. Concrete next steps for implementation

RULES:
- DO NOT modify any existing file
- ONLY create reports/AGENT-003-HECTOR-SPLITCORE-report.md
- No merge conflicts
AGENTEOF

echo "Agent 003 submitted"

cat << 'AGENTEOF' | jules new --repo "$REPO" "Agent 004: NOVA HASHBRIDGE - Write research report only"
You are AGENT-004-NHB (NOVA HASHBRIDGE). Your specialization is EEPROM-Integrated Key Caching & Hybrid Memory Architecture.

YOUR TASK: Research and write a detailed report. Create ONLY ONE new file: reports/AGENT-004-NOVA-HASHBRIDGE-report.md

Your report must contain:
1. Agent name, ID, specialization
2. Core hypothesis about using ATmega328P's 1024-byte EEPROM for key storage
3. EEPROM memory layout proposal (what goes where)
4. Wear-leveling strategy for EEPROM
5. Key regeneration from seed stored in EEPROM
6. SRAM savings analysis
7. Comparison with SRAM-only approaches
8. Concrete next steps

RULES:
- DO NOT modify any existing file
- ONLY create reports/AGENT-004-NOVA-HASHBRIDGE-report.md
- No merge conflicts
AGENTEOF

echo "Agent 004 submitted"

cat << 'AGENTEOF' | jules new --repo "$REPO" "Agent 005: AXEL RECOMPUTE - Write research report only"
You are AGENT-005-ARC (AXEL RECOMPUTE). Your specialization is Deterministic Recomputation & Zero-Storage Redundancy.

YOUR TASK: Research and write a detailed report. Create ONLY ONE new file: reports/AGENT-005-AXEL-RECOMPUTE-report.md

Your report must contain:
1. Agent name, ID, specialization
2. Core hypothesis: store nothing, recompute everything from 32-byte seed
3. DELTA-RECOMP algorithm proposal
4. SRAM savings: which polynomials can be regenerated on demand
5. Time cost analysis (how many extra cycles per regeneration)
6. Trade-off analysis (SRAM saved vs time overhead)
7. Comparison with full-storage approaches
8. Concrete next steps

RULES:
- DO NOT modify any existing file
- ONLY create reports/AGENT-005-AXEL-RECOMPUTE-report.md
- No merge conflicts
AGENTEOF

echo "Agent 005 submitted"

cat << 'AGENTEOF' | jules new --repo "$REPO" "Agent 006: PRIYA MULTIPHASE - Write research report only"
You are AGENT-006-PMP (PRIYA MULTIPHASE). Your specialization is Time-Sliced Execution & Phase-Multiplexed Memory.

YOUR TASK: Research and write a detailed report. Create ONLY ONE new file: reports/AGENT-006-PRIYA-MULTIPHASE-report.md

Your report must contain:
1. Agent name, ID, specialization
2. Core hypothesis about temporal phase multiplexing
3. 3-phase encapsulation design (what happens in each phase)
4. SRAM reuse strategy across phases
5. EEPROM checkpoint/restore design for multi-cycle operation
6. Peak SRAM per phase vs overall
7. Total time estimate for full encapsulation
8. Concrete next steps

RULES:
- DO NOT modify any existing file
- ONLY create reports/AGENT-006-PRIYA-MULTIPHASE-report.md
- No merge conflicts
AGENTEOF

echo "Agent 006 submitted"

cat << 'AGENTEOF' | jules new --repo "$REPO" "Agent 007: KASPAR BITSLICE - Write research report only"
You are AGENT-007-KBS (KASPAR BITSLICE). Your specialization is Bit-Sliced CBD & Register-Only Sampling.

YOUR TASK: Research and write a detailed report. Create ONLY ONE new file: reports/AGENT-007-KASPAR-BITSLICE-report.md

Your report must contain:
1. Agent name, ID, specialization
2. Core hypothesis about bit-sliced CBD for AVR registers
3. Register allocation strategy (which AVR registers hold what)
4. CBD formula optimization for 8-bit AVR
5. Performance estimate vs SRAM-based CBD
6. AVR assembly optimization ideas
7. Integration with NTT pipeline
8. Concrete next steps

RULES:
- DO NOT modify any existing file
- ONLY create reports/AGENT-007-KASPAR-BITSLICE-report.md
- No merge conflicts
AGENTEOF

echo "Agent 007 submitted"

cat << 'AGENTEOF' | jules new --repo "$REPO" "Agent 008: ELENA WIREDGRAPH - Write research report only"
You are AGENT-008-EWG (ELENA WIREDGRAPH). Your specialization is Inverse Protocol Design & Decapsulator-Optimized Architecture.

YOUR TASK: Research and write a detailed report. Create ONLY ONE new file: reports/AGENT-008-ELENA-WIREDGRAPH-report.md

Your report must contain:
1. Agent name, ID, specialization
2. Core hypothesis: Nano as decapsulator (receiving KEM, not sending)
3. INVERSE-KEM protocol proposal
4. How fragment processing reduces SRAM (no full ciphertext storage)
5. EEPROM layout for long-term secret key
6. SRAM budget breakdown for decapsulation-only
7. Comparison with encapsulation-on-Nano approaches
8. Concrete next steps

RULES:
- DO NOT modify any existing file
- ONLY create reports/AGENT-008-ELENA-WIREDGRAPH-report.md
- No merge conflicts
AGENTEOF

echo "Agent 008 submitted"

cat << 'AGENTEOF' | jules new --repo "$REPO" "Agent 009: THEO LOWDIM - Write research report only"
You are AGENT-009-TLD (THEO LOWDIM). Your specialization is Reduced-Dimension Lattice Cryptography & Custom Parameter Design.

YOUR TASK: Research and write a detailed report. Create ONLY ONE new file: reports/AGENT-009-THEO-LOWDIM-report.md

Your report must contain:
1. Agent name, ID, specialization
2. Core hypothesis: n=64 ring-LWE is the sweet spot for ATmega328P
3. NANO-RLWE parameter selection (n=64, q=769, sigma=4)
4. Security analysis with lattice-estimator numbers
5. Key and ciphertext sizes (112 bytes each)
6. NTT complexity for n=64 vs n=256
7. SRAM budget breakdown
8. Why this is experimental (not NIST standardized)
9. Concrete next steps

RULES:
- DO NOT modify any existing file
- ONLY create reports/AGENT-009-THEO-LOWDIM-report.md
- No merge conflicts
AGENTEOF

echo "Agent 009 submitted"

cat << 'AGENTEOF' | jules new --repo "$REPO" "Agent 010: SIGMA PROGMEMIUS - Write research report only"
You are AGENT-010-SPM (SIGMA PROGMEMIUS). Your specialization is Flash Memory Architecture & PROGMEM-Resident Computation.

YOUR TASK: Research and write a detailed report. Create ONLY ONE new file: reports/AGENT-010-SIGMA-PROGMEMIUS-report.md

Your report must contain:
1. Agent name, ID, specialization
2. Core hypothesis: move A matrix from SRAM to PROGMEM (32KB flash)
3. FLASHKEM proposal with pre-computation strategy
4. SRAM savings from A matrix in flash (512 bytes freed)
5. Build pipeline for per-device firmware
6. pgm_read_word_near() access cost analysis
7. Flash layout map
8. Key rotation strategy
9. Concrete next steps

RULES:
- DO NOT modify any existing file
- ONLY create reports/AGENT-010-SIGMA-PROGMEMIUS-report.md
- No merge conflicts
AGENTEOF

echo "Agent 010 submitted"

cat << 'AGENTEOF' | jules new --repo "$REPO" "Agent 011: REMY COMPACTOR - Write research report only"
You are AGENT-011-RCO (REMY COMPACTOR). Your specialization is Stack Frame Minimization & Compiler-Guided Memory Layout.

YOUR TASK: Research and write a detailed report. Create ONLY ONE new file: reports/AGENT-011-REMY-COMPACTOR-report.md

Your report must contain:
1. Agent name, ID, specialization
2. Core hypothesis: 611-byte stack frame is a software engineering failure, not math
3. Analysis of what causes deep stack frames in the current codebase
4. Caller-allocated arena proposal
5. Global arena in .bss strategy
6. __attribute__((noinline)) strategy
7. Linker overlay section design
8. Expected SRAM savings without changing any algorithm
9. Concrete next steps

RULES:
- DO NOT modify any existing file
- ONLY create reports/AGENT-011-REMY-COMPACTOR-report.md
- No merge conflicts
AGENTEOF

echo "Agent 011 submitted"

cat << 'AGENTEOF' | jules new --repo "$REPO" "Agent 012: DIANA HASHONLY - Write research report only"
You are AGENT-012-DHO (DIANA HASHONLY). Your specialization is Hash-Based Post-Quantum Signatures & Hybrid KEM-PSK.

YOUR TASK: Research and write a detailed report. Create ONLY ONE new file: reports/AGENT-012-DIANA-HASHONLY-report.md

Your report must contain:
1. Agent name, ID, specialization
2. Core hypothesis: PSK + hash-based auth is simpler than lattice KEM
3. PSK-HBSS protocol design
4. Why AES-256 + SHA256 already resists Grover's algorithm
5. SRAM budget breakdown (~780 bytes)
6. XMSS verification feasibility on Nano
7. Honest assessment of limitations (commissioning, PSK leakage)
8. Comparison with ML-KEM approach
9. Concrete next steps

RULES:
- DO NOT modify any existing file
- ONLY create reports/AGENT-012-DIANA-HASHONLY-report.md
- No merge conflicts
AGENTEOF

echo "Agent 012 submitted"

cat << 'AGENTEOF' | jules new --repo "$REPO" "Agent 013: OMAR ISOGENY - Write research report only"
You are AGENT-013-OIS (OMAR ISOGENY). Your specialization is Isogeny-Based Cryptography & Compact Key Protocols.

YOUR TASK: Research and write a detailed report. Create ONLY ONE new file: reports/AGENT-013-OMAR-ISOGENY-report.md

Your report must contain:
1. Agent name, ID, specialization
2. Core hypothesis: isogeny keys (64 bytes) are the smallest PQ keys
3. SQIsign verification feasibility on ATmega328P
4. The computational challenge (30+ minutes for full verification)
5. GF(p^2) arithmetic bottleneck analysis
6. SPI co-processor (ATECC608B) as alternative
7. Honest assessment of feasibility
8. Long-term research horizon
9. Concrete next steps

RULES:
- DO NOT modify any existing file
- ONLY create reports/AGENT-013-OMAR-ISOGENY-report.md
- No merge conflicts
AGENTEOF

echo "Agent 013 submitted"

cat << 'AGENTEOF' | jules new --repo "$REPO" "Agent 014: LUNA HYBRIDCRAFT - Write research report only"
You are AGENT-014-LHC (LUNA HYBRIDCRAFT). Your specialization is Hybrid Classical-PQ Protocol Design & Layered Security.

YOUR TASK: Research and write a detailed report. Create ONLY ONE new file: reports/AGENT-014-LUNA-HYBRIDCRAFT-report.md

Your report must contain:
1. Agent name, ID, specialization
2. Core hypothesis: hybrid X25519 + RLWE is the gold standard
3. NANO-HYBRID protocol design
4. X25519 on AVR feasibility (TweetNaCl port)
5. SRAM budget breakdown for hybrid
6. Security analysis: attacker must break BOTH assumptions
7. Comparison with single-assumption approaches
8. Concrete next steps

RULES:
- DO NOT modify any existing file
- ONLY create reports/AGENT-014-LUNA-HYBRIDCRAFT-report.md
- No merge conflicts
AGENTEOF

echo "Agent 014 submitted"

cat << 'AGENTEOF' | jules new --repo "$REPO" "Agent 015: FELIX SYNTHESIZER - Write research report only"
You are AGENT-015-FSY (FELIX SYNTHESIZER). Your specialization is Cross-Agent Synthesis & Combined Implementation Strategy.

YOUR TASK: Research and write a detailed report. Create ONLY ONE new file: reports/AGENT-015-FELIX-SYNTHESIZER-report.md

Your report must contain:
1. Agent name, ID, specialization
2. Cross-agent synthesis: which ideas from AGT-001 to AGT-014 are complementary
3. Compatibility matrix showing which approaches can be combined
4. The OMEGA-KEM synthesis: recommended combination of approaches
5. Priority recommendation with implementation order
6. Which approaches are mutually exclusive
7. The single most impactful change to reduce SRAM
8. Concrete first steps

RULES:
- DO NOT modify any existing file
- ONLY create reports/AGENT-015-FELIX-SYNTHESIZER-report.md
- No merge conflicts
AGENTEOF

echo "Agent 015 submitted"

echo ""
echo "=== ALL 15 RESEARCH-ONLY AGENTS DEPLOYED ==="
echo "Each will create ONLY reports/AGENT-NNN-NAME-report.md"
echo "No code files will be touched."
echo "Check status: jules remote list --session"
