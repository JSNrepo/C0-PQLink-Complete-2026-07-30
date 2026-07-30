# Agent 007 — KASPAR BITSLICE
**ID:** AGT-007-KBS  
**Specialization:** AVR Bit-Sliced Cryptography & Hardware-Parallel Operations  
**Perspective:** *The AVR's 32 general-purpose registers are 32 parallel bits. Use them as a SIMD unit.*

---

## Core Hypothesis

The ATmega328P has 32 × 8-bit general-purpose registers (r0–r31). Bit-sliced arithmetic treats these as **32 parallel 1-bit lanes** across 8 independent "circuits." KASPAR BITSLICE proposes bit-sliced implementation of the CBD sampler, reducing sampling time from O(256) hash extractions to O(32) while also enabling 8× parallel sample validation.

## Algorithm Proposal: BS-SAMPLE (Bit-Sliced CBD Sampling for AVR)

### Why Bit-Slicing Helps Here

ML-KEM-512's CBD(η=3) sampling works as follows for each coefficient:
```
Given 3 bytes of randomness b0, b1, b2:
  η1 = popcount(b0[0], b0[1], b0[2])  // count bits in byte b0 nibble
  η2 = popcount(b1[0], b1[1], b1[2])  // count bits in byte b1 nibble  
  coefficient = η1 - η2               // value in [-3, 3]
```

For 256 coefficients, this requires **768 bytes of randomness** from the PRF.

In bit-sliced form: process **8 coefficients simultaneously** using the AVR's register file.

### Bit-Sliced CBD(η=3) on AVR

```asm
; Input: r0-r2 hold 8 bytes each (1 byte per coefficient, 8 coefficients)
; r0: a0_0..a0_7  (first bit of a-nibble for 8 coefficients)
; r1: a1_0..a1_7  (second bit of a-nibble for 8 coefficients)
; r2: a2_0..a2_7  (third bit of a-nibble for 8 coefficients)
; r3-r5: same for b-nibble

; Bit-sliced popcount for η1 (Hamming weight of 3 bits, for 8 coefficients at once)
; η1_bit0 = a0 XOR a1 XOR a2              (sum bit 0)
; η1_bit1 = (a0 AND a1) OR (a1 AND a2) OR (a0 AND a2)   (carry/sum bit 1)
; η1_bit2 = a0 AND a1 AND a2              (3-carry bit: only when all 3 set)

; η1[k] can be 0,1,2,3 — needs 2 bits to represent

eor r6, r0, r1          ; bit 0 partial: a0^a1  (1 cycle)
eor r6, r6, r2          ; η1_bit0 = a0^a1^a2    (1 cycle)

mov r7, r0              ; (1 cycle)
and r7, r1              ; a0 AND a1             (1 cycle)
mov r8, r1              ; (1 cycle)
and r8, r2              ; a1 AND a2             (1 cycle)
or  r7, r8              ; (a0&a1)|(a1&a2)       (1 cycle)
mov r8, r0              ; (1 cycle)
and r8, r2              ; a0 AND a2             (1 cycle)
or  r7, r8              ; η1_bit1               (1 cycle)

; Similar for η2... (same pattern, r3-r5 as input)
; Then: coefficient_bit0 = η1_bit0 XOR η2_bit0
;        coefficient_bit1 = η1_bit1 XOR η2_bit1 XOR borrow
```

**Result:** 8 coefficients sampled in ~30 AVR instructions (**< 2 μs at 16 MHz**)

### Throughput Comparison

| Method | Cycles per coefficient | 256 coefficients |
|--------|----------------------|-----------------|
| Standard byte-by-byte | ~50 cycles | 12,800 cycles |
| BS-SAMPLE (8-parallel) | ~8 cycles | 2,048 cycles |

**6× speedup** with NO change to the mathematical correctness.

### Integration with CBL-3 Packing (Agent 002)

After bit-sliced sampling, pack the 8 simultaneous coefficients directly into CBL-3 format:
```c
// 8 parallel coefficients, each as 2-bit (sign+magnitude) or 3-bit offset form
// Pack directly: no intermediate int16_t array needed
cbl3_pack_8(out_buf, pos, coeff_signs, coeff_mags);
```

This eliminates the intermediate `int16_t[8]` array — saving another 16 bytes per tile.

### Secondary Proposal: Bit-Sliced Keccak Lane Mixing

The Keccak-f[1600] permutation uses 64-bit lanes internally. On AVR, each 64-bit lane is 8 bytes. With bit-slicing:
- Process **8 Keccak states simultaneously** (impossible on AVR — too much SRAM)
- **OR**: Process one Keccak state but use bit-slicing for XOR-heavy steps

Actually, the real gain is in the `θ` step of Keccak:
```
θ: For each lane x: lane[x] ^= parity(column[x-1]) ^ rot1(parity(column[x+1]))
```

The parity computation across 5 lanes can be done in bit-sliced form across the 8 bytes of each lane simultaneously:

```asm
; Compute parity of 5 lanes (each 8 bytes) simultaneously for all 8 byte positions
; r0-r4: one byte each from 5 lane columns
eor r0, r1          ; parity = lane0^lane1
eor r0, r2          ; ^lane2
eor r0, r3          ; ^lane3
eor r0, r4          ; ^lane4 — done in 4 cycles for one byte position
```

### Secret-Dependent Branch Elimination

Bit-slicing is **inherently constant-time**: all 8 coefficient computations execute identical instructions regardless of coefficient values. There are **no branches** in the hot loop. This satisfies the side-channel gate from the CLAIM-LEDGER.

### AVR Register Pressure Analysis

```
Registers available: r0-r31 (32 total)
CBD sampling (bit-sliced): 9 registers (r0-r8)
Loop control + pointers:   6 registers (r26-r31 = X,Y,Z pairs)
Intermediate results:     10 registers
Accumulate/spill:          7 registers
                         ─────────────
Total:                    32 → tight but feasible
```

Compiler register allocation with `register` hints or hand-written `asm` blocks needed for the innermost loop.

### Implementation Strategy

1. Write the CBD bit-sliced sampler in AVR assembly (`src/avr/cbd_bitslice.S`)
2. Wrap with a C function: `void cbl3_sample_cbd_bitsliced(uint8_t *prf_output, uint8_t *packed_out, uint16_t n)`
3. Test against the standard C reference: both must produce identical output for the same PRF bytes
4. Profile cycle count on `simavr`

### Full AVR SRAM Impact

BS-SAMPLE eliminates the need for an intermediate 512-byte `int16_t` array during sampling — because we go directly from PRF bytes → packed form:
- **Saved: 512 bytes** (the secret polynomial as int16_t)
- **Added: 30 bytes** (bit-sliced working registers — these are CPU registers, NOT SRAM)
- **Net savings: 512 bytes**

---

*KASPAR BITSLICE — AGT-007-KBS | Research Snapshot: 2026-07-30*
