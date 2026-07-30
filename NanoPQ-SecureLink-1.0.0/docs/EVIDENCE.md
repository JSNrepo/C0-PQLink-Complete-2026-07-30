# Evidence and reproducibility

## Test layers

| Layer | Command | What it establishes |
|---|---|---|
| Primitive/session host tests | `make host-test` | SHA-256, HMAC, Ascon vector, protocol agreement, tamper, replay, reset freshness |
| Memory-safety instrumentation | `make sanitizer-test` | ASan and UBSan pass on host implementations and streaming verifiers |
| AVR cross-link | `make evidence` | Flash, static SRAM, EEPROM, sections, disassembly, compiler frame usage |
| AVR execution | `make simulate` | Full compiled enrollment/session/traffic/reset path in avr8js |
| Gate aggregation | `make benchmark` | Rebuilds machine-readable table and fails if a replacement profile exceeds 1,792 B peak or has under 256 B headroom |
| Independent LMS check | `make reference-lms HASH_SIGS=/path/to/hash-sigs` | Cisco RFC 8554 implementation accepts W4/W8 vectors and rejects tamper |

LeakSanitizer is disabled because it cannot inspect `/proc` in the constrained
test container. AddressSanitizer and UndefinedBehaviorSanitizer remain enabled.
The AVR firmware performs no dynamic allocation.

## Independent LMS reference

The comparison used Cisco's `hash-sigs` implementation at commit:

```text
0335491815c908cad85d6035d43785693a4e91f9
```

Observed result:

```text
PASS: Cisco RFC 8554 reference accepted LMS H5/W4 and rejected tamper
PASS: Cisco RFC 8554 reference accepted LMS H5/W8 and rejected tamper
```

The source is not vendored into this package. The test adapter is
`tests/cisco_lms_reference_check.c`.

## Labels

- `PASS`: reproduced host-side result.
- `PASS_SIMULATOR`: linked AVR instructions executed in the target simulator.
- `FAIL`: a gate was measured and not met.
- `PHYSICAL_NANO_RUN_STILL_REQUIRED`: no physical serial device was available
  to this build environment.

## What is not inferred

- simulator cycles are not presented as energy measurements;
- an ATmega1284P or Cortex-M4 paper is not presented as a Nano benchmark;
- a valid test vector is not a certification;
- a toy quantum circuit does not prove or disprove the security of this
  protocol;
- the successful signature-authorized PSK architecture is not described as a
  public-key KEM.

