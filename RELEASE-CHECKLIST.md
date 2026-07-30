# Release checklist

## Reproducibility

- [ ] `npm ci`
- [ ] `make clean && make verify`
- [ ] `make size-report`
- [ ] Demo provisioning generator produces both files
- [ ] Archive excludes `node_modules`, `build`, generated secrets, peer
      configuration, Python cache, and editor files
- [ ] SHA-256 checksum generated for the archive

## Target hardware

- [ ] Named MCU, board, board core, compiler, and flags recorded
- [ ] Linked flash/static RAM totals recorded
- [ ] Worst-case stack high-water recorded with interrupts enabled
- [ ] `tools/verify-avr-constant-time.sh` or equivalent target disassembly
      gate passes
- [ ] Hardware ML-KEM oracle matches independent expected bytes
- [ ] Live peer interop runs on the real packet transport
- [ ] Latency, cycles, energy, airtime, retries, and reboot behavior measured
- [ ] Secure RNG source and failure behavior documented

## Security and operations

- [ ] Protocol reviewed independently
- [ ] Fuzzing covers frame parser and state transitions
- [ ] Timing, power, EM, and fault exposure reviewed for the target
- [ ] Unique PSK provisioning and revocation implemented
- [ ] Public-key rotation and minimum-epoch enforcement implemented
- [ ] A/B journal tested with real power interruption
- [ ] Logging reviewed for secret/plaintext leakage
- [ ] Private vulnerability-reporting contact assigned

## Distribution

- [ ] Real project/repository URL added to `library.properties`
- [ ] Maintainer identity and support policy assigned
- [ ] Arduino IDE ZIP import tested on named boards
- [ ] MicroPython native module built on named ports
- [ ] CMake configure/build/install/consumer tested
- [ ] Apache-2.0 and third-party notices reviewed
- [ ] Claim ledger updated from actual evidence
