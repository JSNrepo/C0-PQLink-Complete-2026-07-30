# Claim ledger

Version: 0.1.0  
Evidence snapshot: 2026-07-30

This ledger controls what may be said in a pitch, README, paper, or demo.
“Pass” means the checked artifact passed the named test; it does not imply
certification or absence of vulnerabilities.

| Claim | Current evidence | Status | Safe wording |
|---|---|---|---|
| Device algorithm is ML-KEM-512 | Eight deterministic encapsulations match `@noble/post-quantum` ciphertext and shared-secret bytes; malformed canonical encoding is rejected | Pass on host | “Exact FIPS 203 ML-KEM-512 bytes in eight independent cross-implementation cases” |
| ML-KEM mathematics were reduced or changed | Code preserves parameters, sampling, NTT, encoding, and wire output | Rejected claim | “We changed scheduling and storage, not the standardized mathematics” |
| Device performs encapsulation | C client reads the public key, obtains local randomness, computes and emits all 768 ciphertext bytes, and receives only ACKs/Finished | Pass by code and interop trace | “Encapsulation stays on the device” |
| Peer is cloud offload | Peer performs ordinary private-key decapsulation and protocol replies | Rejected claim | “Replaceable opposite endpoint; no device computation is offloaded” |
| Whole public key is absent from SRAM | C API uses a byte-read callback; test supplies the key through that callback | Structural pass | “Callback-backed public key can remain in flash” |
| Whole ciphertext is absent from device SRAM | Ciphertext writer streams output; protocol retains one 48-byte fragment | Structural pass | “The device does not allocate a 768-byte ciphertext buffer” |
| Maximum primitive ciphertext write | Oracle asserts five bytes across eight cases | Pass on host | “Largest current ML-KEM writer callback is five bytes” |
| ML-KEM workspace | Current host `sizeof` reports 1,312 bytes; public static bound is 1,344 bytes | Pass for current source layout; not a whole-program fit result | “1,312-byte current core state within a 1,344-byte public workspace” |
| Entire package fits an ATmega328P/2 KB SRAM | Reproducible named Nano ELF uses 20,118 flash bytes and 2,016 static SRAM bytes before stack; `connect()` stack frame is 611 bytes | **Rejected for current implementation** | “The current implementation does not fit; RPE-32 is an unimplemented replacement hypothesis” |
| AVR signed multiplication has no secret branches | Source uses four unsigned 8×8 products and branchless two’s-complement correction; object-level AVR audit finds no conditional instruction in the correction function | Partial disassembly pass; linked helper and physical leakage still pending | “Fixed-schedule correction passed its object-disassembly gate; constant-time claim remains pending” |
| Ascon-AEAD128 format | Known-answer bytes, round trip, tamper rejection, and failure zeroization pass | Pass on host | “SP 800-232 Ascon-AEAD128 implementation passes the included known-answer gate” |
| Full live connection interoperates | C client completes ML-KEM, Finished, Ascon record, and response with an independent JavaScript peer in both exposed session modes | Pass on host | “Cross-language live-session interoperability passes in both modes” |
| Packet loss is handled | Test drops first Challenge, fragment-7 ACK, first Peer Finished, and first data response; exact request replay receives a cached response | Pass for injected cases | “Bounded recovery passes four injected-loss cases, including an idempotent data retry” |
| Active tampering is rejected transactionally | Forged fragment, Finished, and data record are rejected; receive chain/sequence do not advance | Pass for injected cases | “Injected tampering is rejected without receive-state advance” |
| Replay protection | Exact next inbound sequence required; duplicate record test passes | Pass in-session | “In-session ordered replay rejection passes” |
| Cross-reboot replay protection | Requires a fresh secure nonce/session and no restoration of live key/sequence state | Assumption/integration gate | “Requires correct reboot/session lifecycle” |
| Downgrade journal survives torn write | Explicit first provisioning cannot overwrite a readable journal; authenticated A/B unit test recovers the old valid record and rejects downgrade | Pass on host model | “Software journal separates initialization, rejects policy rollback, and detects simulated torn writes” |
| Physical anti-rollback | No hardware monotonic counter is implemented | Not provided | “Use platform monotonic storage or peer minimum-epoch enforcement” |
| Arduino library imports | Arduino-style C++ wrapper compiles; a named ATmega328P ELF was linked | Build-path pass, usability fail due SRAM and placeholder adapters | “The Nano build path links, but the current binary is not runnable within SRAM” |
| MicroPython package imports | Python facade syntax-compiles; native user-module source and build descriptors are present | Partial pass | “Native MicroPython integration is packaged; named-port firmware build remains pending” |
| Energy or battery improvement | No power measurements | Not measured | Do not quote energy, battery-life, or microjoule claims |
| Latency or cycle improvement | No target timing/cycle measurements | Not measured | Do not quote speedup or millisecond claims |
| Radio behavior on LoRaWAN/NB-IoT | Core frames are at most 96 bytes; no field-stack airtime or duty-cycle test | Structural only | “Core frame size is bounded; link overhead and duty-cycle compliance require target testing” |
| Security proof or audit | No formal analysis or independent audit | Not done | “Experimental custom protocol using standardized primitives” |
| NIST approval | NIST standardized ML-KEM and Ascon, not C0-PQLink | Primitive-only | “Uses FIPS 203 ML-KEM-512 and SP 800-232 Ascon-AEAD128” |
| Medical/grid/automotive readiness | No domain certification, hazard analysis, or target validation | Not ready | “Research prototype; not approved for safety-critical deployment” |
| Quantum technology platform | Qiskit 2.5.1 and Qiskit Aer 0.17.2 run pinned, seeded Shor/RSA-15 and Grover/four-bit circuits with raw counts | Pass for local ideal simulation | “Real Qiskit Aer toy-circuit simulation; not quantum hardware” |
| Quantum circuit optimization | Levels 0–3 and Grover iterations 0–5 were executed; raw distributions and circuit metrics are retained | Pass for the named toy circuits | “Measured circuit optimization on toy attack demonstrations” |

## Reproduce the evidence

```sh
npm ci
make clean
make verify
make size-report
sh tools/verify-avr-constant-time.sh
```

`make verify` covers unit vectors, independent ML-KEM oracle comparisons,
cross-language live-session interoperability, injected loss/tamper, Arduino
API/example host compilation, header mirrors, and sanitizers.

The AVR audit intentionally returns exit code 77 when `avr-gcc` and
`avr-objdump` are absent. A skipped audit is pending evidence, not a pass.

## Next evidence required

1. Select one named 8-bit board and toolchain version.
2. Produce linked map, disassembly, flash/RAM totals, and worst-case stack
   high-water under interrupts.
3. Run deterministic oracle and live-peer tests on hardware.
4. Measure cycles, elapsed time, energy, retries, and radio airtime.
5. Run nonce/RNG health and reboot tests.
6. Perform timing/power/EM/fault review.
7. Fuzz parsers and state transitions.
8. Commission independent cryptographic protocol review.
