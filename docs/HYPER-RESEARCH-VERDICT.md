# Nano live-session PQC: hyper-research verdict

Evidence snapshot: 2026-07-30  
Target: classic Arduino Nano / ATmega328P, 16 MHz, 32 KiB flash, 2 KiB SRAM  
Scope: post-quantum key establishment for live network connections

## Executive decision

The research does **not** support selecting ThreeBears, LightSaber, BIKE,
RLizard, a compressed multivariate proposal, or a reduced-dimension custom
lattice as a ready-to-use Nano KEM.

It supports this two-track decision:

1. **Usable package track:** retain exact FIPS 203 ML-KEM-512 and develop a new
   low-memory *execution algorithm* for it. The provisional name is
   **RPE-32**: Recomputation-Packed Execution with 32-coefficient tiles. It
   changes storage, scheduling, recomputation, AVR arithmetic, and streaming,
   but not the ML-KEM parameters, equations, randomness, encodings, public key,
   ciphertext, or shared secret. It is not a new KEM and must produce the exact
   standard output for every input.
2. **Cryptographic research track:** study compact KEM mathematics separately,
   using CTRU-Light and DAWN as the strongest recent comparison points. Any
   altered construction remains behind an `EXPERIMENTAL` interface and is not
   described as standardized, production-ready, or quantum-secure.

This is the most defensible novelty for a scientific review: fitting the
standardized KEM by changing its execution graph, and proving the claim on the
actual 2 KiB target. Inventing weaker parameters merely to obtain a demo would
score poorly on methodology, feasibility, and cryptographic correctness.

## Evidence labels

Every claim in this report has one of four meanings:

- **Verified:** reproduced from the current package or stated directly in a
  primary standard, candidate specification, paper, or artifact.
- **Derived:** arithmetic or protocol consequence calculated from verified
  inputs.
- **Hypothesis:** technically motivated design that still needs code and target
  measurements.
- **Pending:** required evidence that does not yet exist.

Published performance results are not treated as measurements of C0-PQLink.
Results from an ATmega1284P, ATmega4808, Cortex-M4, x86-64, FPGA, or simulator
are not silently transferred to the ATmega328P.

## Phase-one mark strategy

The review has 50 marks. Each category needs a visible artifact rather than a
verbal claim.

| Category | Marks | Judge-visible evidence |
|---|---:|---|
| Methodology | 10 | Primary-source candidate screen; claim labels; controlled benchmark protocol; threat model; falsifiable release gates |
| Approach | 5 | Exact ML-KEM execution optimization + compact live-session wire profile + Ascon traffic layer; experimental math isolated |
| Feasibility | 5 | ATmega328P linked map, stack high-water, instruction cycles, full simulated exchange, physical Nano exchange, and explicit `DOES NOT FIT` results |
| Quantum technology utilization | 10 | Pinned Qiskit and Qiskit Aer platform; real circuits; backend/method, seeds, transpiled circuit, shots, raw counts, versions, and one-command reproduction |
| Quantum optimization | 10 | Measured transpiler levels, circuit depth/size/two-qubit gates, Grover iteration sweep, success probability, and honest resource scaling |
| Implementation | 10 | Importable Arduino library, reference peer, deterministic oracle tests, live encrypted sensor exchange, fault injection, and benchmark artifacts |

The quantum-platform marks and the cryptographic-security claim are deliberately
separate. Qiskit demonstrates how quantum algorithms attack toy classical
problems. ML-KEM security is evaluated through standards, cryptanalysis,
test-vector equivalence, interoperability, and target execution—not by saying a
small simulator failed to break it.

## Hard target reality check

### SRAM

The Nano has 2,048 bytes of SRAM for all of the following at once:

- `.data` and `.bss`;
- the cryptographic workspace;
- nested call frames and saved registers;
- interrupt stack;
- transport buffers;
- Arduino core state;
- sensor/application state.

Therefore, “the primitive uses less than 2 KiB” is insufficient. The acceptance
target is:

- crypto arena plus its deepest stack: **at most 1,024 bytes**;
- complete representative program peak: **at most 1,792 bytes**;
- operational margin under the tested interrupt configuration: **at least
  256 bytes**.

The current C0-PQLink implementation does not meet that gate. Its reconstructed
AVR link uses 2,016 bytes of static SRAM before runtime stack, and its current
`connect()` frame is 611 bytes. This is a measured rejection of the present
implementation, not a rejection of the RPE-32 hypothesis.

### Wire size

The aspirational “public key + ciphertext below 256 bytes” target is not
supported by a credible security-category-1 KEM found in this review.

Examples:

- ML-KEM-512: 800-byte encapsulation key + 768-byte ciphertext = 1,568 bytes.
- LightSaber: 672 + 736 = 1,408 bytes.
- BabyBear: 804 + 917 = 1,721 bytes.
- BIKE Level 1: 1,541 + 1,573 = 3,114 bytes.
- HQC-128: 2,249 + 4,497 = 6,746 bytes.
- CTRU-Light: 640 + 512 = 1,152 bytes.
- DAWN-beta reports 964 bytes combined at NIST category I, but has no Nano
  implementation.

The correct network requirement is consequently **bounded authenticated
fragments**, not “one KEM in one universal IoT MTU.” IEEE 802.15.4 allows a
127-byte physical frame; an example protected configuration leaves 81 bytes
after link overhead. LoRaWAN and Bluetooth LE payload budgets vary with region,
data rate, and link mode. A library must ask the transport for its actual
budget.

### Integer arithmetic

Avoiding floating point is correct, but it is not a new advantage over
ML-KEM. FIPS 203 already forbids floating-point arithmetic. The innovation must
be measured reduction in live storage, instructions, and energy while
preserving the exact integer algorithm.

## Keep, correct, or discard

| Input idea | Verdict | Reason |
|---|---|---|
| Use fixed-width 8/16-bit integer C first | **Keep** | Appropriate for portable profiling and AVR cross-compilation |
| Stream public keys and ciphertexts | **Keep** | Removes full artifacts from SRAM; network fragmentation still needs authentication and recovery |
| Trade recomputation for RAM | **Keep** | Public matrix expansion and deterministic secret sampling can be repeated with fixed schedules |
| Learn from NTRU-like small-modulus arithmetic | **Keep as prior art** | CTRU-Light provides relevant 8-bit modular arithmetic and NTT evidence |
| Learn from power-of-two moduli and packed coefficients | **Keep as prior art** | Saber-family work shows useful packing/reduction ideas, not Nano fit |
| Learn from bit-sliced code-based operations | **Keep as a technique** | XOR/bit packing is AVR-friendly, but full code dimensions and artifacts remain large |
| Start with `simavr`/Microchip Studio before hardware | **Keep** | Useful for exact linked bytes and instruction cycles; physical execution remains mandatory |
| BabyBear fits below 1 KiB | **Discard** | The published AVR work reports about 1.7 KiB for CPA-only BabyBearEphem and about 2.4 KiB for the CCA KEM |
| ThreeBears processes independent 312-byte blocks | **Correct** | Its core is arithmetic modulo a roughly 3,120-bit integer; “312-byte streaming blocks” is not a security or Nano-fit result |
| LightSaber is proven fast on ATmega328P | **Discard** | Its arithmetic is attractive, but published constrained results are mainly Cortex-M4; a recent AVR artifact reports much larger stack |
| Saber lost only because it was slower on Intel | **Discard** | NIST found Kyber and Saber similar; MLWE being better studied than MLWR and updated security estimates also influenced selection |
| BIKE is still a Round-4 algorithm under evaluation | **Discard** | NIST selected HQC in 2025; BIKE was not selected |
| BIKE has automatically tiny RAM because it flips bits | **Discard** | Bit operations are simple, but large code dimensions, decoder state, keys, ciphertexts, and side-channel behavior remain |
| RLizard is a proven Nano-ready KEM | **Discard** | Compact artifacts do not establish a CCA-secure, 2 KiB implementation; available embedded measurements do not show Nano fit |
| Seed-compressed multivariate public keys solve the problem | **Discard as current solution** | Multivariate finalists were signatures, public-key expansion remains substantial, and Rainbow was practically broken |
| Use NTRU dimension 251 or lower | **Discard without analysis** | Dimension is a security parameter, not an optimization knob; no credible category-1 estimate was found for the proposed reduction |
| Wrap an experimental KEM in AES-256 | **Correct** | A traffic cipher does not preserve key-establishment confidentiality if the KEM breaks |
| Use a hybrid for a security hedge | **Keep with correction** | Combine independent key-establishment secrets through a reviewed KDF; a high-entropy per-device PSK can be a separate symmetric hedge but is not a PQ/traditional KEM hybrid |
| Make the live channel completely stateless | **Discard** | AEAD nonces cannot repeat under a key and receivers need replay state |
| Make reboot handling reset-safe | **Keep** | Destroy the old context, establish a fresh session with sound entropy, and maintain peer replay/downgrade policy |
| A failed Qiskit attack proves ML-KEM security | **Discard** | Simulation failure is not a proof; use standards, cryptanalysis, exact vectors, and implementation evidence |

## Candidate screen

Sizes are bytes and refer to the named parameter/version. “RAM” is the best
relevant result located, not a uniform benchmark. A different platform or
security variant is identified explicitly.

| Candidate | Status on snapshot date | Encapsulation key | Ciphertext | Relevant memory evidence | Nano decision |
|---|---|---:|---:|---|---|
| ML-KEM-512 | FIPS 203 | 800 | 768 | Current C0 core arena 1,344; current linked static SRAM 2,016 before stack | **Retain math; replace execution schedule** |
| BabyBear | NIST Round-2 candidate, not selected | 804 | 917 | AVR CCA path about 2.4 KiB total; low-memory encapsulation 1,735 B; ATmega1284 | Reject as package KEM; retain RPS/Karatsuba lessons |
| LightSaber | NIST finalist, not selected | 672 | 736 | NIST cites sub-4-KiB Cortex-M4 implementations; 2026 AVR artifact reports 11,262 B encapsulation stack for its build | Reject for Nano |
| BIKE Level 1 | NIST Round-4 candidate, not selected | 1,541 | 1,573 | NIST comparison is x86-64; no credible Nano result located | Reject for Nano |
| HQC-128 | Selected for future NIST standardization | 2,249 | 4,497 | Large artifacts and polynomial state; no Nano fit | Reject for Nano, keep as diversity benchmark |
| Classic McEliece 348864 | Not selected by NIST | 261,120 | 96 | Tiny ciphertext is outweighed by a public key far beyond Nano flash | Reject |
| NTRU Prime sntrup653 | Research/standardization elsewhere; not FIPS | 994 | 897 | Simple conservative structure, but no sub-1-KiB Nano encapsulation evidence located | Reject for Nano |
| CTRU-Light | TCHES 2026 research KEM | 640 | 512 | Best listed encapsulation stack 2,463 B, code 26,572 B on ATmega1284P; targets have 6/16 KiB SRAM | Research comparator only |
| DAWN-beta | ASIACRYPT 2025 research KEM | — | — | 964 B combined; optimized desktop evidence, no AVR implementation located | Research comparator only |
| TiMER / SMAUG-T | Research/Korean PQC candidate | 672* | 608* | No reproducible Nano result; current project warns that an earlier implementation contained bugs | Research comparator only |
| TiGER | Research KEM | 928* | 1,152* | No credible Nano result located | Reject for Nano |
| Sable (Scabbard) | Research KEM | 608 | 672 | Cortex-M4 encapsulation stack 5,928 B in reported implementation | Reject for Nano |
| Espada (Scabbard) | Research KEM | 1,072 | 1,088 | Cortex-M4 encapsulation stack 1,960 B, before a 2-KiB application budget | Reject for Nano |
| PQ-IoTCrypt IoT-Basic | Research BRLWE proposal | inconsistent reporting | inconsistent reporting | About 5.1 KiB encryption RAM on a 168-MHz Cortex-M4F; claimed PQ estimate about 73 bits | Reject |
| Custom multivariate compression | No reviewed KEM selected | unknown | unknown | No concrete secure parameter set or Nano implementation | Do not invent for release |
| Custom QC-MDPC reduction | No reviewed parameter set | unknown | unknown | Decoder failure/security/side-channel analysis absent | Do not invent for release |

\* Version-sensitive research value; must be rechecked against the exact source
commit used by any future benchmark.

### What the screen actually teaches

The useful pattern is not “rejected candidates fit.” It is:

- memory-optimized implementations repeatedly trade cycles for recomputation;
- packed coefficients matter more than source-level `const`;
- AVR-specific multiplication and flash reads matter;
- public artifacts can be streamed;
- a CCA KEM usually needs more memory than a CPA encryption core;
- platform labels such as “constrained” often mean 4 KiB, 6 KiB, or more—not
  the Nano's 2 KiB;
- small keys do not guarantee a small working set;
- simple mathematical operations do not guarantee small dimensions or a safe
  decoder.

## Selected production research path: RPE-32

### Boundary

RPE-32 is a provisional name for a storage and execution algorithm for exact
ML-KEM-512 encapsulation. It is **not** a modified lattice assumption or a
smaller parameter set.

FIPS 203 explicitly permits a conforming implementation to replace specified
steps with a mathematically equivalent procedure that produces the correct
output for every input, including all parameter and randomness values. This is
the standards boundary RPE-32 must satisfy.

Unchanged:

- \(n=256\), \(q=3329\), module rank \(k=2\);
- \(\eta_1=3\), \(\eta_2=2\), \(d_u=10\), \(d_v=4\);
- SHA3/SHAKE functions and domain separation;
- sampling distributions;
- public-key validation;
- 800-byte encapsulation key;
- 768-byte ciphertext;
- 32-byte shared secret;
- fresh 32-byte encapsulation randomness;
- all FIPS encodings and failure behavior.

Changed:

- polynomial representation while live;
- order of public-data expansion;
- tiling of NTT and inverse NTT;
- deterministic regeneration of ephemeral polynomials;
- placement of constants in program flash;
- callback-backed public-key reads;
- ciphertext streaming;
- phase overlays between KEM and protocol state.

### Hypothesized arena

| Object | Representation | Bytes |
|---|---|---:|
| Accumulator polynomial | 256 canonical coefficients × 12 bits | 384 |
| One ephemeral polynomial | 256 values packed into 3 bits | 96 |
| NTT working tile | 32 signed 16-bit coefficients | 64 |
| Keccak state and cursor | Compact AVR representation | 208 |
| Public seed \(\rho\) | Bytes | 32 |
| Encapsulation coins / sampler key | Bytes | 32 |
| Packing, counters, reductions | Conservative allowance | 24 |
| **Provisional arena** | Before measured compiler alignment | **840** |

This 840-byte figure is a **derived allocation target**, not a measured peak.
It excludes call stack, ISR stack, transport buffers, and persistent client
state. The implementation passes only if the linked Nano measurement satisfies
the complete 1,024-byte crypto and 1,792-byte program gates.

### Tiled forward NTT

The standard in-place transform stores a 256-coefficient `int16_t` polynomial.
RPE-32 instead stores the small time-domain secret in 3-bit form and produces
32 NTT-domain coefficients at a time:

1. Sample one secret polynomial into the 96-byte packed representation.
2. Select one 32-coefficient output tile.
3. Recompute the fixed first transform layers needed by that tile directly
   from packed input coefficients.
4. Run the remaining local layers in the 64-byte tile.
5. Stream the corresponding public matrix or public-key coefficients.
6. Base-multiply and add into the 12-bit packed accumulator.
7. Erase the tile and continue.

All loop bounds and addresses must depend only on public indices. Secret
coefficients cannot control branches, memory addresses, or instruction count.
Recomputation deliberately increases cycles. The exact multiplication count
will be taken from instrumented code; no speedup is claimed in advance.

### Packed inverse NTT

The inverse transform operates on the 12-bit accumulator:

1. Perform layers that remain inside a 32-coefficient tile using the 64-byte
   buffer.
2. Write every reduced coefficient back to the packed accumulator.
3. Perform cross-tile layers as fixed strided butterflies with two packed
   reads and two packed writes.
4. Stream the noise addition, message addition where applicable, compression,
   and ciphertext encoding.

Canonical reduction on every packed write avoids needing wider persistent
coefficients. A host equivalence harness must compare every intermediate stage
against a straightforward implementation before the AVR path is trusted.

### Regeneration schedule

ML-KEM-512 encryption needs two ephemeral vector polynomials multiple times.
Keeping both expanded polynomials would consume 1,024 bytes alone. RPE-32
regenerates each from the same derived coins and public nonce whenever needed.

The schedule is:

```text
validate and hash encapsulation key
derive K and sampler coins from fresh m and H(ek)

for each u row:
    clear packed accumulator
    for each ephemeral-vector column:
        regenerate packed secret polynomial
        tiled NTT
        stream/regenerate the public matrix polynomial
        accumulate products
    packed inverse NTT
    regenerate and add e1
    compress and stream u

clear packed accumulator
for each public-key vector column:
    regenerate packed secret polynomial
    tiled NTT
    stream public-key polynomial from flash/callback
    accumulate products
packed inverse NTT
regenerate and add e2 and encoded message
compress and stream v
erase m, coins, secrets, accumulator, and tile
```

The public matrix XOF may be restarted for each tile and skip public
coefficients, trading cycles for state simplicity. A later optimization may
retain XOF state if the measured peak still passes.

### Why this path outranks a new small KEM

- Exact FIPS bytes provide a definitive independent oracle.
- The security parameters are not weakened to hit a demo budget.
- Peers require no custom cryptography.
- Existing ML-KEM migration work remains usable.
- Failure of the memory hypothesis is easy to detect from the linked target.
- The novelty is concrete and measurable: a new bounded-memory execution
  graph, not an unsupported security claim.

### Novelty caution

“RPE-32 is novel” remains **pending** until a focused prior-art and patent
search covers streamed/stack-optimized Kyber and ML-KEM implementations,
recomputation schedules, blocked NTTs, packed in-place transforms, and AVR
implementations. The safe current wording is:

> We are evaluating a provisional recomputation-and-packing schedule for exact
> ML-KEM-512 on the ATmega328P.

## Nano live-session wire profile

The current 84-byte Challenge and 96-byte maximum frame do not fit an
81-byte protected IEEE 802.15.4 budget. A Nano profile should use a compact
8-byte header and negotiate the payload budget.

Provisional frames:

| Frame | Calculation | Size |
|---|---|---:|
| Client Hello | 8 header + 1 suite + 8 device ID + 8 epoch + 16 client nonce + 16 tag | 57 |
| Peer Challenge | 8 header + 1 suite + 8 epoch + 16 key ID + 16 peer nonce + 16 tag | 65 |
| Ciphertext fragment, 32-byte payload | 8 header + 3 fragment fields + 32 data + 16 tag | 59 |
| Ciphertext fragment, 16-byte payload | 8 header + 3 fragment fields + 16 data + 16 tag | 43 |
| Finished | 8 header + 16 tag | 24 |
| Selective ACK | 8 header + compact bitmap/fields + 16 tag | target ≤ 32 |

At 32 bytes per fragment, a 768-byte ML-KEM ciphertext uses 24 fragments. The
larger number of packets is a real energy and latency cost and must appear in
benchmarks. The peer, not the Nano, reassembles the full ciphertext.

Each fragment is authenticated before acceptance. Fragment index, total count,
length, suite, key ID, epoch, and both nonces are bound into the handshake
transcript. Retries are bounded and exact duplicates are idempotent.

## Reset-safe, not stateless

A secure live channel cannot be completely stateless:

- AEAD nonce reuse under one key is forbidden;
- a receiver needs a sequence or replay window;
- downgrade policy needs an epoch or peer-enforced minimum;
- retransmission needs a transaction identity.

The Nano-friendly rule is:

1. Never restore a live traffic key with reset sequence counters.
2. After reboot, erase the old context and establish a fresh session.
3. Require a sound fresh device nonce and peer nonce.
4. Let the peer retain the larger replay cache.
5. Keep only the current send/receive sequence and minimal session state on the
   Nano.
6. Enforce downgrade resistance with authenticated peer policy or suitable
   monotonic storage; do not wear EEPROM on every packet.

## Entropy gate

The ATmega328P has no documented true random-number generator. FIPS 203
requires fresh 32-byte randomness for every encapsulation from an approved
random-bit generator with 128-bit strength for ML-KEM-512.

Therefore:

- an RNG callback is mandatory;
- a floating `analogRead()` input is not accepted as an entropy claim;
- failure to obtain randomness fails closed;
- the demo must use a documented external entropy source, secure radio RNG, or
  measured entropy source feeding a DRBG;
- source health tests, seed lifecycle, reseeding, and reboot behavior must be
  recorded;
- deterministic seeds are used only for known-answer tests, never a live
  connection.

## Side-channel gate

Small memory does not imply constant time. The release requires:

- no secret-dependent branch or lookup in source;
- linked AVR disassembly review;
- fixed-instruction unsigned multiplication with branchless sign correction;
- constant public-key validation behavior where practical;
- stack and bounds instrumentation;
- timing distributions on the physical Nano;
- power/EM testing sufficient to avoid claiming resistance from source
  inspection alone;
- compiler-version pinning because instruction selection can reintroduce
  leakage.

The result may be described as “fixed-schedule implementation under test” until
physical leakage evidence exists.

## Experimental compact-KEM track

CTRU-Light is the closest published 8-bit design located. It contributes useful
small-modulus, NTT-friendly Montgomery/Barrett, and K-RED2X ideas. It still
does not establish Nano fit: its listed best Ascon-based encapsulation stack is
2,463 bytes on an ATmega1284P, before the application and radio stack.

DAWN is the strongest located answer to the wire-size direction. DAWN-beta
reports a 964-byte combined public key and ciphertext at category I, still far
above 256 bytes, and no AVR implementation was found.

A future custom KEM branch may proceed only after it includes:

- a complete specification and domain separation;
- category-1 parameter estimates using current lattice/code estimators;
- decryption-failure analysis;
- IND-CCA transform and proof assumptions;
- deterministic test vectors;
- two independent implementations;
- malformed-ciphertext and failure-oracle analysis;
- side-channel design;
- prominent `EXPERIMENTAL — NOT STANDARDIZED` labeling;
- no automatic selection by the production package.

Low-dimensional NTRU, compressed multivariate keys, and reduced QC-MDPC codes
are research questions, not implementation shortcuts.

## Quantum technology platform

### Selected platform

- **SDK:** Qiskit 2.5.1
- **Simulator:** Qiskit Aer 0.17.2
- **Backend:** `AerSimulator`
- **Current ideal method:** statevector
- **Reproducibility:** fixed simulator and transpiler seeds, fixed shots,
  version capture, transpiled circuit metrics, raw counts in JSON

This is genuine quantum-circuit simulation on a classical machine. It is not an
IBM Quantum hardware run. If hardware access is added later, its job/backend,
calibration snapshot, queue date, transpiled circuit, and raw result must be
stored separately; simulator and hardware results must not be mixed.

### Current real experiments

| Experiment | Platform result | Correct claim |
|---|---|---|
| Compiled Shor order finding for toy RSA-15 | 8 qubits, 4,096 shots; recovered order 4 and factors 3 and 5; reconstructed toy private exponent and decrypted ciphertext | Demonstrates the factorization attack workflow on a deliberately tiny compiled instance |
| Grover search over a four-bit key | 4 qubits, 16 candidates, 3 iterations; target `1011` measured 3,926/4,096 times | Demonstrates quadratic search behavior in a deliberately tiny space |

Neither experiment breaks RSA-2048, ECC-256, Ascon-128, or ML-KEM-512.

### Quantum optimization evidence

The optimization study has been executed with 4,096 shots per point. All
circuits were transpiled to the fixed `rz/sx/x/cx` basis.

| Circuit | Level | Depth | Operations | CX gates | Measured success |
|---|---:|---:|---:|---:|---:|
| Shor/RSA-15 | 0 | 328 | 578 | 180 | 50.27% factor-producing shots |
| Shor/RSA-15 | 1 | 324 | 535 | 180 | 50.27% |
| Shor/RSA-15 | 2 | 316 | 470 | 174 | 50.02% |
| Shor/RSA-15 | 3 | 316 | 470 | 174 | 50.02% |
| Grover, three iterations | 0 | 200 | 364 | 84 | 95.85% target |
| Grover, three iterations | 1 | 172 | 238 | 84 | 95.85% |
| Grover, three iterations | 2 | 167 | 232 | 84 | 95.85% |
| Grover, three iterations | 3 | 167 | 232 | 84 | 95.85% |

The seeded success counts can differ between optimization levels when the
transpiled circuit changes the simulator's sampling path. The relevant result
is that the solution distribution remains correct while circuit cost falls.

The separate level-3 Grover sweep measured:

| Iterations | Depth | Operations | CX gates | Target hits / 4,096 | Probability |
|---:|---:|---:|---:|---:|---:|
| 0 | 4 | 16 | 0 | 266 | 6.49% |
| 1 | 59 | 88 | 28 | 1,946 | 47.51% |
| 2 | 113 | 160 | 56 | 3,707 | 90.50% |
| 3 | 167 | 232 | 84 | 3,937 | **96.12%** |
| 4 | 221 | 304 | 112 | 2,413 | 58.91% |
| 5 | 275 | 376 | 140 | 493 | 12.04% |

The measured optimum at three iterations agrees with
\(\lfloor\frac{\pi}{4}\sqrt{16}\rfloor=3\), and the decline after the optimum
shows Grover over-rotation rather than a monotonic “more gates is better”
story.

The complete JSON preserves every raw distribution, seeds, versions, circuit
metrics, and observed timings. The timing fields are environment-specific;
the circuit metrics and seeded counts are the primary reproducible evidence.
Future noisy simulation may be added only when its noise model and calibration
source are recorded, and it must remain separate from these ideal results.

This earns “quantum optimization” marks through measured circuit evidence
while the RPE-32 work earns embedded optimization evidence. Shor's production
resource requirements and Grover's square-root scaling are not represented by
these toy qubit counts.

## Benchmark protocol

### Compared operations

Compare equivalent client-side key-establishment operations:

- RPE-32 exact ML-KEM-512 encapsulation;
- current C0 streamed ML-KEM-512;
- a conventional/reference ML-KEM-512 port;
- BabyBear memory-efficient CCA KEM if the source can be built under its
  license;
- LightSaber;
- CTRU-Light where the artifact license and target port allow;
- HQC-128 as a standards-diversity comparison, expected not to fit;
- an AVR classical ECDH implementation;
- RSA public-key operation as a legacy comparison.

Ascon-AEAD128 is benchmarked separately as traffic protection. It is not
mislabelled as a KEM.

### Controlled build

- exact MCU: `atmega328p`;
- exact clock: 16 MHz;
- pinned `avr-gcc`, binutils, Arduino core, and linker script;
- same optimization policy, with `-Os` and LTO variants reported separately;
- same entropy/test-vector boundary;
- same public-key placement policy where supported;
- warm-up policy and run count recorded;
- raw compiler maps, `.su` files, disassembly, simulator traces, and serial
  logs retained.

### Metrics

| Metric | Required method |
|---|---|
| Flash | Linked `.text + .data` from AVR ELF/map |
| Static SRAM | Linked `.data + .bss + noinit` |
| Peak stack | Canary/high-water on target plus call-graph cross-check |
| Total peak SRAM | Static + measured stack + active transport/application buffers |
| Cycles | Instruction-level simulation and hardware timer/GPIO cross-check |
| Latency | Physical Nano wall-clock distribution |
| Energy | Measured voltage/current integration; no datasheet multiplication presented as measured |
| Wire bytes | Captured authenticated frames including retries and headers |
| Radio airtime | Calculated from captured frames and verified against the named radio configuration |
| Correctness | Exact deterministic oracle and peer decapsulation |
| Robustness | Loss, replay, malformed key/ciphertext, tamper, reboot, RNG failure |

If a candidate cannot link or exceeds SRAM, report `DOES NOT FIT`. Do not assign
it an invented time or energy value.

## Release gates

The Nano package is not complete until all gates pass:

1. Exact equality with FIPS 203/independent ML-KEM results for official and
   generated deterministic inputs.
2. Public-key canonical validation through the streamed interface.
3. Named Arduino Nano board build with pinned toolchain.
4. Complete representative peak at or below 1,792 SRAM bytes.
5. Crypto arena plus deepest stack at or below 1,024 bytes.
6. At least 256 bytes measured interrupt/application headroom.
7. Complete handshake and Ascon sensor record in instruction-level simulation.
8. The same exchange on a physical Nano and replaceable peer.
9. Working entropy source and failure test.
10. Compact frames within the declared transport budget.
11. Selective retry, tamper, replay, reboot, and downgrade tests.
12. Linked disassembly and physical timing/leakage evidence.
13. Raw benchmark artifacts for every comparison.
14. Reproducible Qiskit platform and circuit-optimization artifacts.
15. Independent cryptographic/protocol review before production use.

## Judge-facing claim

The strongest claim supported by this research is:

> C0-PQLink is developing a reproducible, importable live-session security
> package for the 2 KiB ATmega328P. Its production path preserves exact FIPS
> 203 ML-KEM-512 while testing a new recomputation-packed execution schedule,
> authenticates compact loss-tolerant fragments, and uses NIST Ascon for live
> traffic. Its quantum evidence consists of real, pinned Qiskit Aer circuits
> and raw results, while all Nano fit, performance, energy, and side-channel
> claims remain gated by measured target evidence.

Do not say:

- “we invented a NIST-approved KEM”;
- “all rejected NIST algorithms fit the Nano”;
- “Qiskit proved ML-KEM secure”;
- “AES protects the session if the KEM breaks”;
- “the channel is stateless”;
- “the current package already fits.”

## Primary-source ledger

Standards and engineering guidance:

- [FIPS 203: ML-KEM](https://nvlpubs.nist.gov/nistpubs/fips/nist.fips.203.pdf)
- [NIST SP 800-232: Ascon-based lightweight cryptography](https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-232.pdf)
- [NIST IR 8413: third-round PQC report](https://nvlpubs.nist.gov/nistpubs/ir/2022/NIST.IR.8413.pdf)
- [NIST IR 8309: second-round PQC report](https://nvlpubs.nist.gov/nistpubs/ir/2020/NIST.IR.8309.pdf)
- [NIST IR 8545: fourth-round PQC report](https://nvlpubs.nist.gov/nistpubs/ir/2025/NIST.IR.8545.pdf)
- [NIST PQC project status](https://csrc.nist.gov/projects/post-quantum-cryptography)
- [NIST SP 800-227: KEM recommendations](https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-227.pdf)
- [IETF constrained-PQC draft](https://datatracker.ietf.org/doc/draft-ietf-pquip-pqc-hsm-constrained/)
- [RFC 9958: PQC for engineers](https://datatracker.ietf.org/doc/html/rfc9958)
- [RFC 9794: PQ/traditional hybrid terminology](https://www.rfc-editor.org/rfc/rfc9794.html)
- [RFC 8613: OSCORE nonce and replay lifecycle](https://www.rfc-editor.org/rfc/rfc8613.html)
- [RFC 9139: IEEE 802.15.4 payload discussion](https://www.rfc-editor.org/rfc/rfc9139.html)
- [NIST SP 800-90B: entropy sources](https://nvlpubs.nist.gov/nistpubs/specialpublications/nist.sp.800-90b.pdf)
- [ATmega328P datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/Atmel-7810-Automotive-Microcontrollers-ATmega328P_Datasheet.pdf)

Candidate and implementation evidence:

- [Saber specification](https://www.esat.kuleuven.be/cosic/pqcrypto/saber/files/saberspecround1.pdf)
- [ThreeBears on 8-bit AVR](https://orbilu.uni.lu/bitstream/10993/48811/1/CARDIS2020.pdf)
- [ThreeBears specification](https://www.shiftleft.org/papers/threebears/threebears-spec.pdf)
- [CTRU-Light paper](https://eprint.iacr.org/2026/248)
- [CTRU-Light AVR artifact](https://github.com/whYBeKim/CTRU-Light-and-PQC-on-AVR)
- [DAWN compact NTRU KEM](https://eprint.iacr.org/2025/1520)
- [Scabbard study](https://arxiv.org/html/2409.09481)
- [TiGER](https://eprint.iacr.org/2022/1651)
- [SMAUG-T current project page](https://kpqc.cryptolab.co.kr/smaug-t)
- [NTRU Prime](https://ntruprime.cr.yp.to/)
- [PQ-IoTCrypt article record](https://www.sciencedirect.com/science/article/pii/S2542660524003329)

The source ledger is a snapshot. Research schemes and artifacts can change;
future benchmarks must pin exact specifications and commits.
