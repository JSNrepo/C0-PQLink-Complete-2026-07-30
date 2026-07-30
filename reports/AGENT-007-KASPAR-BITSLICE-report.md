# Agent Report: Bit-Sliced CBD & Register-Only Sampling

## 1. Agent Identity
- **Agent Name**: KASPAR BITSLICE
- **ID**: AGENT-007-KBS
- **Specialization**: Bit-Sliced Centered Binomial Distribution (CBD) & Register-Only Sampling

## 2. Core Hypothesis
The core hypothesis is that implementing Centered Binomial Distribution (CBD) sampling entirely within the AVR's 32 general-purpose registers—utilizing bit-slicing techniques—will significantly reduce the latency and energy consumption of polynomial coefficient generation. By eliminating costly SRAM load/store operations (which take 2 cycles each) and operating in parallel across multiple bit slices using fast logical operations (1 cycle each), we can drastically accelerate Kyber/Dilithium noise sampling on 8-bit AVR microcontrollers.

## 3. Register Allocation Strategy
To keep the entire operation in-register and avoid SRAM spilling, we must carefully allocate the 32 available registers (r0-r31):
- **r0-r1**: Temporary multiplication / zero registers.
- **r2-r9 (8 registers)**: Random input buffer (XOF output bits directly loaded).
- **r10-r17 (8 registers)**: Bit-sliced Boolean variables for parallel bit computation.
- **r18-r25 (8 registers)**: Accumulator registers holding the partially and fully computed CBD coefficients (in bit-sliced form).
- **r26-r31 (X, Y, Z pointers)**: Reserved for NTT output buffer pointers and Keccak state pointers.

## 4. CBD Formula Optimization for 8-bit AVR
For $\eta=2$ (commonly used in Kyber), the standard CBD formula is $a_1 + a_2 - b_1 - b_2 \pmod q$.
In a bit-sliced representation across 8 bits, this translates to parallel logic operations. Instead of doing arithmetic additions and subtractions sequentially, we use Boolean gates:
- Compute bit 0: XOR operations on the input bits.
- Compute bit 1: Majority gates (AND/OR) for carry generation.
- Subtraction mod $q$: Implemented as addition of $q - b_1 - b_2$.

By replacing arithmetic operations with sequences of `eor`, `and`, and `or` instructions applied to whole registers, we compute 8 coefficients simultaneously.

## 5. Performance Estimate vs. SRAM-Based CBD
- **SRAM-Based CBD**: Involves reading PRNG output byte-by-byte (2 cycles per read), computing coefficients sequentially (~15-20 cycles per coefficient), and writing to SRAM (2 cycles per write). Total: ~25 cycles per coefficient.
- **Bit-Sliced Register-Only CBD**: 8 coefficients are computed in parallel. Loading 8 registers takes 16 cycles, Boolean logic takes ~20 cycles total, and storing takes 16 cycles. Total: ~52 cycles for 8 coefficients, or ~6.5 cycles per coefficient.
- **Expected Speedup**: Approximately 3.8x faster execution with significantly reduced SRAM bus contention.

## 6. AVR Assembly Optimization Ideas
- **Instruction Level Parallelism**: Interleave memory load instructions (from Keccak state) with logical bit-slicing instructions to hide any pipeline stalls and avoid data hazards, though AVR is single-issue.
- **Use of `swap` and `bst`/`bld`**: Exploit the `swap` (nibble swap) and bit-copy instructions to efficiently route bits from the XOF output byte into the bit-sliced layout.
- **Zero Register Optimization**: Dedicate one register (e.g., `r1`) to be constantly zero to speed up moves and comparisons without needing an `ldi`.
- **Minimize Branching**: Unroll loops entirely for the 8-coefficient chunk computation. The logic is strictly linear, which is ideal for AVR without a branch predictor.

## 7. Integration with NTT Pipeline
To maximize throughput, the CBD sampler can be fused with the first layer of the Number Theoretic Transform (NTT) pipeline:
- As soon as a register containing 8 finalized coefficients is ready, instead of writing it directly back to SRAM, pass it directly into the first butterfly operation of the NTT.
- This "lazy store" approach eliminates an entire pass over the SRAM, combining polynomial sampling and the first stage of NTT transformation.

## 8. Concrete Next Steps
1. **Implement Bit-Sliced Logic Prototype**: Write a C/C++ prototype of the bit-sliced logic to verify mathematical correctness against the standard $\eta=2$ CBD reference implementation.
2. **Write Inline Assembly Core**: Translate the verified Boolean logic into pure AVR assembly, rigorously tracking register liveness.
3. **Benchmarking**: Measure the exact cycle count of the standalone assembly function using an AVR simulator (e.g., simavr).
4. **Integration Test**: Connect the assembly core to the SHA3/Keccak state and test the register-only handoff.
5. **Develop NTT Fusion**: Design the interface to pass the generated coefficients directly into the first NTT butterfly stage.
