# Report: MIRA PACKWRIGHT (AGENT-002-MPW)

## 1. Agent Name, ID, Specialization
- **Agent Name:** MIRA PACKWRIGHT
- **ID:** AGENT-002-MPW
- **Specialization:** Ternary Polynomial Compression & Coefficients Bit-Level Packing

## 2. Core Hypothesis
ML-KEM-512 secret polynomials drawn from a centred binomial distribution CBD(η₁=3) take values in **{-3, -2, -1, 0, 1, 2, 3}**. Since there are only 7 possible values, storing these coefficients in standard 16-bit integers (`int16_t`) wastes 13 bits per coefficient. The core hypothesis is that by compressing these values into a **3-bit signed form** using a custom 96-byte packed buffer, secret polynomial storage on constrained architectures like the ATmega328P can be reduced by a factor of 5.3× without compromising correctness or performance.

## 3. Detailed CBL-3 (Coefficient Bit-Level 3-bit Packing) Proposal
CBL-3 (Centred Binomial Level-3 Packing) proposes the following packing scheme:

Each coefficient $c \in \{-3, -2, -1, 0, 1, 2, 3\}$ is biased by +3:
- Stored Value = $c + 3$
- The resulting range is $[0, 6]$, which fits entirely within 3 bits.

For ML-KEM-512, there are 256 coefficients per polynomial:
- 256 coefficients × 3 bits = 768 bits = **96 bytes**.

**Byte-Level Layout (Little-Endian Bit Order):**
The 3-bit values cross byte boundaries.
- `byte[k]` holds:
  - bits [2:0] $\rightarrow$ `coeff[k*2+0] + 3`
  - bits [5:3] $\rightarrow$ `coeff[k*2+1] + 3`
  - bits [7:6] $\rightarrow$ lower 2 bits of `coeff[k*2+2] + 3`
- `byte[k+1]` holds:
  - bit [0] $\rightarrow$ upper 1 bit of `coeff[k*2+2] + 3`
  - ...and so on.

## 4. SRAM Savings Analysis vs Standard `int16_t` Representation

| Approach | Storage per polynomial | Total for 4 polynomials ($s_0, s_1, e_0, e_1$) |
|----------|------------------------|------------------------------------------------|
| Standard `int16_t` | 512 bytes | 2048 bytes |
| NTT-domain `int16_t` | 512 bytes | 2048 bytes |
| 4-bit packed | 128 bytes | 512 bytes |
| **CBL-3 (Proposed)** | **96 bytes** | **384 bytes** |

By storing all four polynomials simultaneously as 3-bit packed arrays, we avoid regeneration loops and save 1664 bytes of SRAM compared to standard `int16_t` storage. This drastically reduces the memory footprint to just 384 bytes, perfectly aligning with the constrained 2048-byte SRAM of the ATmega328P.

## 5. NTT on Packed Representation Feasibility
The packed representation is highly compatible with tiled NTT algorithms (e.g., RPE-32).
- **No re-store needed:** Butterfly operations can extract one coefficient at a time, process it using the 3-bit unpack scheme, and perform computations directly.
- **Fast Extraction:** A constant-time O(1) extraction function can pull coefficients from the 96-byte packed buffer using simple bitwise operations (shift and mask). 
- **Workspace Sharing:** Extracted values are loaded into a smaller shared 64-byte NTT tile workspace. The constant-time unpacking function scales elegantly without needing full decompression into an intermediate 512-byte buffer.

## 6. Estimated Performance on AVR
On the ATmega328P (AVR architecture):
- 3-bit extraction uses only basic instructions: `LD`, `LDD`, `AND`, `LSR`.
- The extraction routine requires approximately **8 AVR instructions** per coefficient.
- At 16 MHz, this runs in ~500 ns.
- For 256 coefficients per polynomial, the total extraction overhead is approximately **128 μs**.
- This overhead is negligible compared to the hundreds of thousands of cycles required for Keccak calls.
- Furthermore, since the polynomials do not need to be dynamically regenerated via SHAKE (since they all fit in SRAM), we avoid expensive Keccak re-initialization entirely, potentially resulting in a net **gain** in speed.

## 7. Comparison with Existing Approaches in `ideas/` Folder
Compared to other proposed memory-saving techniques for ML-KEM on AVR:
- **AGENT-010-SIGMA-PROGMEMIUS (Flash-Resident Matrix):** Focuses on moving the public matrix to flash memory (PROGMEM). CBL-3 complements this perfectly by compressing the secret and error polynomials in SRAM.
- **AGENT-005-AXEL-RECOMPUTE (Recomputation):** Proposes re-generating polynomials via SHAKE dynamically to save SRAM. CBL-3 completely obsoletes the need for heavy recomputation because the 5.3x compression allows all polynomials to reside in SRAM at once, saving significant Keccak compute cycles.
- **RPE-32 (Tile-Based NTT from Core Design):** Focuses on chunking the NTT processing. CBL-3 feeds seamlessly into RPE-32's 64-byte tile buffers, combining SRAM savings from both the storage layer and the processing layer.

## 8. Concrete Next Steps
1. **Develop Unpack Primitives:** Write and optimize the constant-time, branchless C/AVR assembly macro `cbl3_get(const uint8_t *buf, uint8_t i)` for 3-bit extraction.
2. **Modify CBD Sampler:** Implement a CBD sampler that directly encodes output coefficients into the 96-byte CBL-3 packed buffer format without allocating a temporary `int16_t` array.
3. **Integrate with RPE-32:** Wire the `cbl3_get` function to feed directly into the RPE-32 tile buffer during NTT computation.
4. **Unit Testing:** Add a suite of tests in `tests/test_cbl3.c` to verify round-trip packing/unpacking and compatibility with existing Keccak output vectors.
5. **Cycle Profiling:** Profile the extraction overhead on physical AVR hardware to confirm the ~128 μs penalty projection.
