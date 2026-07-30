# AGENT-014-LUNA-HYBRIDCRAFT Report

## 1. Agent Identity
- **Agent Name:** LUNA HYBRIDCRAFT
- **ID:** AGENT-014-LHC
- **Specialization:** Hybrid Classical-PQ Protocol Design & Layered Security

## 2. Core Hypothesis
The core hypothesis is that a hybrid approach combining X25519 (classical elliptic curve cryptography) and RLWE (Ring Learning with Errors, a post-quantum cryptographic assumption) is the gold standard for secure communication, particularly in resource-constrained environments. This layered security ensures that even if one underlying mathematical assumption is broken by future quantum computers or unforeseen classical cryptanalysis, the overall system remains secure.

## 3. NANO-HYBRID Protocol Design
The NANO-HYBRID protocol is designed for microcontrollers and edge devices. It fuses classical and post-quantum key encapsulation mechanisms (KEMs) into a unified handshake:
- **Phase 1: Classical Key Exchange:** Standard X25519 is executed to establish a shared secret $K_{classical}$.
- **Phase 2: Post-Quantum Encapsulation:** An RLWE-based KEM (such as Kyber-512 reduced or a lightweight variant) is executed to establish a shared secret $K_{PQ}$.
- **Phase 3: Key Derivation:** Both shared secrets are fed into a robust Key Derivation Function (KDF), e.g., HKDF-SHA256, to produce the final session key: $K_{session} = KDF(K_{classical} || K_{PQ})$.
This design ensures forward secrecy against both classical and quantum adversaries.

## 4. X25519 on AVR Feasibility (TweetNaCl Port)
Implementing X25519 on an AVR architecture (e.g., ATmega328P) is highly feasible when leveraging optimized libraries like TweetNaCl. TweetNaCl provides a compact, auditable implementation of Curve25519.
- **Code Size:** Fits within the limited flash memory (typically <10KB for the X25519 portion).
- **Execution Time:** While a scalar multiplication on an 8-bit AVR takes significant cycles (roughly 15-30 million cycles, or a few seconds at 16MHz), it is acceptable for infrequent key exchanges or session establishments.
- **Portability:** TweetNaCl's minimal dependency on standard libraries makes it trivial to port to bare-metal AVR environments.

## 5. SRAM Budget Breakdown for Hybrid
A typical AVR microcontroller (like ATmega328P) has only 2KB of SRAM. A strict memory budget is essential for the hybrid approach:
- **X25519 State (TweetNaCl):** ~200-300 bytes (for large integer arithmetic and buffers).
- **RLWE (Lightweight/Reduced Kyber):** ~800-1000 bytes (requires careful in-place NTT operations and streamed matrix/vector generation to avoid large allocations).
- **KDF & Hashing (SHA256):** ~200 bytes context and buffers.
- **Stack & Application Overhead:** ~400-500 bytes.
**Total SRAM:** ~1.6KB - 2KB. This tight budget requires meticulous optimization, particularly streaming PRNGs and performing in-place calculations to stay within the limits without stack overflow.

## 6. Security Analysis
The primary strength of the hybrid approach is that an attacker must break BOTH mathematical assumptions to compromise the session key:
- **Assumption A (Classical):** The hardness of the Elliptic Curve Discrete Logarithm Problem (ECDLP) on Curve25519.
- **Assumption B (Post-Quantum):** The hardness of the Ring Learning with Errors (RLWE) problem.
If a Cryptographically Relevant Quantum Computer (CRQC) breaks ECDLP using Shor's algorithm, the RLWE component maintains security. If a new classical attack severely weakens RLWE, the X25519 component provides a proven safety net. The combination using a strong KDF guarantees that partial compromise yields no advantage.

## 7. Comparison with Single-Assumption Approaches
| Feature | Single Classical (e.g., X25519) | Single PQ (e.g., RLWE) | Hybrid (X25519 + RLWE) |
| :--- | :--- | :--- | :--- |
| **Quantum Threat** | Vulnerable (Shor's Algorithm) | Secure (Theoretical) | Secure |
| **Classical Confidence** | Very High (Years of scrutiny) | Moderate (Relatively new) | Very High |
| **Performance (AVR)** | Slow but manageable | Fast (NTT-based), but memory heavy | Slowest, highest memory demand |
| **Bandwidth** | Very Low (~32 bytes) | High (~800-1000+ bytes) | Very High |
| **Fail-safe** | None | None | Yes (Requires dual-break) |

Single-assumption approaches present an all-or-nothing risk profile. The hybrid model mitigates the "harvest now, decrypt later" threat while hedging against potential weaknesses in nascent PQ algorithms.

## 8. Concrete Next Steps
1. **Prototype X25519 on AVR:** Integrate and benchmark the TweetNaCl X25519 port on target hardware (e.g., Arduino Uno/ATmega328P) to establish baseline timing and SRAM usage.
2. **Optimize RLWE Implementation:** Refine the RLWE KEM for AVR, focusing on streaming matrix generation and in-place Number Theoretic Transform (NTT) to minimize SRAM consumption.
3. **Develop NANO-HYBRID Harness:** Write the overarching protocol logic that sequentially executes X25519 and RLWE, feeding outputs into the KDF.
4. **Memory Profiling:** Perform rigorous dynamic memory analysis (e.g., stack painting) to ensure the 2KB SRAM limit is never breached during the combined handshake.
5. **Security Audit:** Conduct a side-channel (power analysis/timing) evaluation on the AVR implementation, as resource-constrained devices are highly susceptible to physical attacks.