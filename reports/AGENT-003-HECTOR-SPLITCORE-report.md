# Report: Alternative Ring/Lattice Design & Hardware-Aligned Parameter Selection

## 1. Agent Details
**Name:** HECTOR SPLITCORE  
**ID:** AGENT-003-HSC  
**Specialization:** Alternative Ring/Lattice Design & Hardware-Aligned Parameter Selection  

## 2. Core Hypothesis
The core hypothesis is that standard lattice-based algorithms like ML-KEM-512 are heavily over-provisioned for the extreme constraints of 8-bit microcontrollers such as the ATmega328P. By utilizing a custom Key Encapsulation Mechanism (KEM) based on smaller ring parameters (specifically $n=128$ and $q=2048$), we can dramatically reduce the SRAM and cycle count requirements while providing an acceptable, albeit reduced, security margin suitable for highly constrained, short-lived ephemeral key exchanges in localized networks.

## 3. CTRU-Light Algorithm Proposal & Parameter Justification
**CTRU-Light** is a proposed lightweight variant of NTRU/Kyber-like KEMs optimized for ultra-constrained environments.

* **Parameter Set:** $n=128, q=2048$.
* **Polynomial Ring:** $R_q = \mathbb{Z}_q[X]/(X^{128} + 1)$.
* **Error Distribution:** Centered Binomial Distribution (CBD) with $\eta=2$.

**Justification:**
- **$n=128$:** Reduces the polynomial size to exactly 128 coefficients. This directly quarters the memory requirement per polynomial compared to $n=256$, fitting comfortably within constrained SRAM.
- **$q=2048$ ($2^{11}$):** Allows each coefficient to be stored in a standard 16-bit integer (2 bytes), utilizing 11 bits of precision. This avoids the complexity of odd moduli like 3329 (used in Kyber) and allows for fast modulo operations via bitwise AND (`x & 0x07FF`).
- **Bitwise Modulo:** Using a power-of-two (or near power-of-two) modulo heavily optimizes modular reductions on an 8-bit AVR architecture, completely sidestepping expensive division or Barrett reductions.

## 4. SRAM Budget Breakdown for ATmega328P
The ATmega328P possesses a mere 2048 bytes (2 KB) of SRAM.

| Component | Size per Polynomial | Total Size (Bytes) | % of Total SRAM |
| :--- | :--- | :--- | :--- |
| Secret Key ($s$) | 1 poly $\times$ 128 coeffs $\times$ 2 bytes | 256 B | 12.5% |
| Public Key ($t, A$) | 1 poly ($t$) + Seed for $A$ (32 B) | 288 B | 14.1% |
| Shared Secret / Ciphertext | Encoded format | ~128 B | 6.25% |
| Working Memory (Buffers) | 2 polys for arithmetic (NTT-free) | 512 B | 25.0% |
| Stack & System Overhead | Stack variables, interrupts | 256 B | 12.5% |
| **Total Peak SRAM** | | **~1440 B** | **~70%** |

This leaves approximately 600 bytes (30%) for application logic and communication stacks (e.g., I2C, SPI), which is a viable operational margin.

## 5. Comparison with ML-KEM-512 on AVR Constraints
| Feature | ML-KEM-512 | CTRU-Light ($n=128, q=2048$) |
| :--- | :--- | :--- |
| $n$ (Ring dimension) | 256 | 128 |
| Modulus $q$ | 3329 | 2048 |
| Polynomial Size | 512 bytes | 256 bytes |
| Modulo Arithmetic | Requires Barrett/Montgomery | Fast Bitwise AND (`& 0x07FF`) |
| Multiplication | NTT (complex logic/memory) | NTT-free Convolution (Toom-Cook/Karatsuba) |
| SRAM Viability on 328P | Exceeds limits or causes stack overflow | Comfortably fits (~70% peak) |

ML-KEM-512 is too heavy for the 328P. Even highly optimized implementations struggle to perform a full key exchange without overwriting the stack due to the size of $n=256$ and the complex NTT buffers required.

## 6. Security Analysis
**Honest Assessment:**
The parameters $n=128$ and $q=2048$ represent a significant compromise in cryptographic security compared to standard NIST PQC levels.
- **Estimated Security Level:** ~40-60 bits of classical security, and significantly less against a quantum adversary equipped with a relevant-sized QPU.
- **Vulnerability:** Highly susceptible to lattice reduction attacks (e.g., BKZ) due to the small dimension $n=128$.
- **Applicability:** CTRU-Light must NOT be used for long-term data encryption. It is solely designed for short-lived ephemeral key exchanges in highly localized environments (e.g., smart home sensors, automotive localized networks) where the data loses value faster than the time/cost required to break the 50-bit security margin.

## 7. Why NTT-free Convolution Matters for 8-bit MCUs
The Number Theoretic Transform (NTT) is highly efficient on 32-bit ARM or x86 processors, but it relies on an odd prime modulus (like $q=3329$) and complex memory access patterns (butterfly operations).
On an 8-bit AVR architecture:
1. **Memory Access Overhead:** Butterfly operations require frequent non-sequential SRAM accesses. Since AVR has limited pointer registers (X, Y, Z), constantly swapping pointers for NTT permutations creates massive cycle overhead.
2. **Modulo Arithmetic:** An odd modulus requires Barrett or Montgomery reduction for every multiplication and addition inside the NTT, costing multiple cycles per operation.
3. **NTT-Free Alternative:** By using a power-of-two modulus ($q=2048$), we can use traditional polynomial multiplication (e.g., Karatsuba or Toom-Cook). While asymptotically slower ($O(n^{1.58})$ vs $O(n \log n)$), for a small $n=128$, the constant factors and the avoidance of complex modular reductions make NTT-free convolution significantly faster and far more memory-efficient on an 8-bit MCU.

## 8. Concrete Next Steps for Implementation
1. **Write Polynomial Arithmetic Core:** Implement assembly-optimized (AVR-ASM) Karatsuba multiplication for 128-coefficient polynomials with 16-bit elements.
2. **Implement Fast Modulo:** Hardcode the `& 0x07FF` reduction in the inner loop of the multiplication.
3. **Develop CBD Sampler:** Create a lightweight Centered Binomial Distribution sampler using a minimal Keccak/SHA-3 permutation (e.g., Keccak-f[200] or a lightweight stream cipher like ChaCha8) to generate noise.
4. **Integration and Testing:** Wrap the arithmetic into the CTRU-Light KeyGen, Encaps, and Decaps functions. Profile SRAM usage and cycle counts on a physical ATmega328P to validate the 1440-byte peak memory hypothesis.
