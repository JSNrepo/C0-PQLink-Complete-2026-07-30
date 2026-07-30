# Benchmark methodology and results

Snapshot: 2026-07-30  
Target: ATmega328P, 16 MHz, 32 KiB flash, 2,048 bytes SRAM  
Compiler: bundled AVR GCC from pinned `quirkbot-avr-gcc` 2.0.4  
Simulator: avr8js 0.21.0

## Acceptance gates

- whole linked program peak SRAM: at most 1,792 bytes;
- measured SRAM headroom: at least 256 bytes;
- no signature buffer in SRAM;
- full enrollment, handshake, encrypted traffic, tamper, replay, and reset path
  must execute;
- physical Nano result remains a separate required gate.

## Same-target results

| Profile | Cryptographic role | Artifact bytes | Flash | Static SRAM | Executed stack | Peak SRAM | Headroom | Authorization at 16 MHz | Status |
|---|---|---:|---:|---:|---:|---:|---:|---:|---|
| C0 ML-KEM-512 | KEM each session | 800-byte pk + 768-byte ct | 20,118 | 2,016 | ≥611 | ≥2,627 | ≤−579 | Not runnable | Failed link/runtime bound |
| LMS H5/W4 | Rare PQ authorization + PSK session | 56-byte pk + 2,348-byte sig | 18,188 | 587 | 767 | 1,354 | 694 | 6.596 s | AVR simulation pass |
| LMS H5/W8 | Rare PQ authorization + PSK session | 56-byte pk + 1,292-byte sig | 18,188 | 587 | 767 | 1,354 | 694 | 47.092 s | AVR simulation pass |
| SLH-DSA-SHA2-128s | Rare PQ authorization + PSK session | 32-byte pk + 7,856-byte sig | 19,268 | 597 | 767 | 1,364 | 684 | 23.638 s | AVR simulation pass |

The old ML-KEM number is a conservative lower bound: static memory plus its
largest compiler-reported `connect()` frame. It excludes interrupts, transport,
and further nested calls.

The replacement profiles execute a complete linked AVR image. Peak stack is the
maximum of direct minimum-stack-pointer tracking and an `0xA5` SRAM watermark.
Peak SRAM is linked `.data + .bss + .noinit` plus the measured executed stack.
No interrupts are enabled in this polling firmware.

## What the W4/W8 failure teaches

W8 looked attractive because its signature is 1,056 bytes smaller than W4.
Actual AVR execution reverses that choice:

- W4: 105,538,287 cycles;
- W8: 753,474,158 cycles;
- W8 is approximately 7.14 times slower.

The cause is the maximum Winternitz chain length: 15 for W4 versus 255 for W8.
The default therefore uses W4. W8 remains a selectable wire-optimized profile.

## Cross-platform prior-art screen

These are source values from different implementations and hardware. They are
not plotted as though they were Nano benchmarks.

| Candidate | Role/status | Key + ciphertext/signature | Most relevant constrained evidence | Decision |
|---|---|---:|---|---|
| ML-KEM-512 | FIPS 203 KEM | 1,568 | Current Nano build requires ≥2,627 B SRAM | Reject current implementation |
| BabyBear CCA | Unselected KEM | 1,721 | 1,735 B low-memory encapsulation on ATmega1284 | No whole Nano session margin |
| LightSaber | Unselected finalist KEM | 1,408 | Published constrained builds exceed the Nano budget | Reject |
| BIKE L1 | Unselected code KEM | 3,114 | No credible ATmega328P fit located | Reject |
| HQC-128 | Selected backup KEM | 6,746 | Large artifacts and decoder state | Reject |
| NTRU Prime sntrup653 | Non-FIPS KEM | 1,891 | No sub-1-KiB Nano encapsulation located | Reject |
| CTRU-Light | 2026 research KEM | 1,152 | 2,463 B encapsulation stack on ATmega1284P | Research lesson only |
| LMS H5/W4 | NIST hash signature | 2,404 | This project: 1,354 B full executed peak | Selected for authorization |
| SLH-DSA-SHA2-128s | FIPS 205 signature | 7,888 | This project: 1,364 B full executed peak | Stateless fallback |

This comparison led to the architectural decision: do not pretend an
authorization signature is a KEM. Use it where a signature is sufficient, then
derive sessions from an independently provisioned secret. Deployments that
require public-key key establishment must use a larger MCU or continue
researching an exact low-memory KEM.

## Reproduce

```bash
npm ci --cache .npm-cache
make benchmark
```

Raw evidence:

- `evidence/avr-lms-w4-e2e.json`
- `evidence/avr-lms-w8-e2e.json`
- `evidence/avr-slh-e2e.json`
- `evidence/avr-*-size.txt`
- `evidence/avr-*-sections.txt`
- `evidence/avr-*-stack-frame-summary.txt`
- `evidence/avr-*-disassembly.txt`
- `evidence/same-target-benchmark.csv`

