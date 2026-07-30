# Prebuilt artifacts

The release ZIP contains:

| File | Purpose |
|---|---|
| `artifacts/firmware/nanopq-lms-w4.hex` | Default flash image |
| `artifacts/firmware/nanopq-lms-w4.elf` | Symbols and debugging |
| `artifacts/firmware/nanopq-lms-w4.map` | Link evidence |
| `artifacts/firmware/nanopq-lms-w8.*` | Wire-optimized LMS alternative |
| `artifacts/firmware/nanopq-slh.*` | Stateless FIPS 205 alternative |
| `artifacts/firmware/factory-reset.eep` | Clear the two demo EEPROM journal slots |
| `artifacts/host/nanopq-peer-linux-x86_64` | Prebuilt Linux x86-64 serial peer |

Rebuilding is preferred. The archive-level `SHA256SUMS` covers every source,
evidence, vector, and prebuilt artifact.

