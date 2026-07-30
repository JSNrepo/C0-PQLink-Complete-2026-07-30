# AXEL RECOMPUTE (AGENT-005-ARC) Report

## 1. Agent Identity
- **Agent Name:** AXEL RECOMPUTE
- **Agent ID:** AGENT-005-ARC
- **Specialization:** Deterministic Recomputation & Zero-Storage Redundancy

## 2. Core Hypothesis
**Store nothing, recompute everything from a 32-byte seed.** The fundamental premise of this approach is that memory (especially SRAM) is a far more constrained and expensive resource in embedded and limited computing environments than computation cycles. By retaining only a highly entropic 32-byte cryptographic seed (e.g., from a TRNG or established root of trust), we can deterministically regenerate any intermediate state, polynomial matrix, or verification key on the fly. This effectively trades compute time for memory space, leading to near-zero storage requirements for runtime variables.

## 3. DELTA-RECOMP Algorithm Proposal
The DELTA-RECOMP algorithm is designed to manage this computation-for-memory trade-off efficiently:

1. **Seed Initialization:** A 32-byte master seed is securely loaded into a protected register.
2. **State Derivation (The Forward Pass):** Instead of storing the full application state, the state is divided into logical epochs or checkpoints. An overarching PRF (Pseudo-Random Function) like SHAKE-256 is used to expand the seed into specific parameters for the current epoch.
3. **On-Demand Regeneration:** When a specific variable or polynomial matrix $A$ is needed for an operation (e.g., in a lattice-based cryptographic scheme), the system inputs the base seed and the specific index of the required data into the PRF.
4. **Delta Caching:** For variables that are accessed very frequently in tight loops, a small, ephemeral "delta cache" is maintained. It stores only the diffs (deltas) from the base generated polynomial, allowing for rapid localized operations. This cache is flushed immediately after the operation completes.
5. **Garbage-less Execution:** As soon as an intermediate value is consumed, its memory is overwritten. No persistent storage is maintained beyond the 32-byte seed and the delta cache.

## 4. SRAM Savings
By adopting DELTA-RECOMP, significant SRAM savings are realized. Specifically, the following polynomials and matrices typically stored in SRAM can be regenerated on demand:
- **Public Matrix $A$:** In schemes like Kyber or Dilithium, the public matrix $A$ is generated from a seed (often $\rho$). Instead of expanding this seed and storing the entire matrix $A$ in SRAM (which can take several kilobytes), each polynomial within $A$ is generated row-by-row or element-by-element precisely when it needs to be multiplied with a secret vector.
- **Intermediate NTT representations:** Number Theoretic Transform (NTT) intermediates can be recomputed from base seeds if the operation is interrupted, rather than maintaining full state arrays across context switches.
- **Ephemeral Keypairs:** Instead of storing generated keypairs in memory while waiting for encapsulation/decryption, the seed that generated them is kept, and the keys are re-derived at the exact moment of use.

## 5. Time Cost Analysis
Regenerating polynomials on demand incurs a computational penalty:
- **Hash/XOF Expansion:** Expanding a seed using SHAKE-128/256 typically costs around 10-20 cycles per byte on a modern ARM Cortex-M4 microcontroller.
- **Polynomial Generation:** Generating a single polynomial of 256 coefficients (e.g., using rejection sampling from the XOF output) takes approximately 5,000 to 10,000 extra cycles per polynomial.
- **Overall Penalty:** If a matrix multiplication requires generating a $4 \times 4$ matrix of polynomials on the fly rather than reading from SRAM, the time overhead is roughly 16 $\times$ 7,500 = 120,000 extra cycles per matrix operation.

## 6. Trade-off Analysis
The trade-off hinges on the specific constraints of the target hardware:
- **SRAM Saved:** ~3 to 4 KB per matrix $A$, plus additional savings from not storing intermediate vectors. Total SRAM reduction can be up to 70-80% for certain cryptographic implementations.
- **Time Overhead:** An increase of approximately 100,000 to 150,000 cycles per major operation. At a 100 MHz clock rate, this translates to an extra 1-1.5 milliseconds of execution time.
- **Conclusion:** For environments where SRAM is strictly limited (e.g., < 16 KB total available memory), saving 4 KB is mission-critical, making the 1.5ms latency penalty highly acceptable. For high-throughput servers, this trade-off is unfavorable.

## 7. Comparison with Full-Storage Approaches
| Metric | Full-Storage Approach | AXEL RECOMPUTE (Zero-Storage) |
| :--- | :--- | :--- |
| **SRAM Usage (State)** | High (Multiple KB) | Near-Zero (32 bytes + ephemeral buffers) |
| **Execution Time** | Fast (Memory-bound) | Slower (Compute-bound) |
| **Energy Consumption** | Low computation power | Higher due to continuous PRF hashing |
| **Security Surface** | Large (many secrets in memory) | Minimal (only the 32-byte seed must be protected) |

Full-storage approaches optimize for speed and battery life at the expense of chip area (SRAM). The Zero-Storage approach optimizes for minimal chip area and a reduced memory footprint, offering a smaller attack surface for memory-dumping attacks.

## 8. Concrete Next Steps
1. **Implement Prototype:** Develop a proof-of-concept C implementation of the DELTA-RECOMP algorithm targeting the ARM Cortex-M4 architecture.
2. **Benchmark PRF:** Perform cycle-accurate profiling of SHAKE-128 and SHAKE-256 on the target hardware to refine the time cost estimates.
3. **Delta Cache Tuning:** Experiment with different sizes for the ephemeral delta cache to find the optimal balance between minor SRAM usage and significant cycle savings in tight loops.
4. **Integration Testing:** Integrate the zero-storage matrix generation into an existing open-source post-quantum cryptography library (e.g., pqm4) and run standard test vectors to ensure deterministic correctness.
