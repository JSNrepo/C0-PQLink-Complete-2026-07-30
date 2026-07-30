# Agent 004 — NOVA HASHBRIDGE
**ID:** AGT-004-NHB  
**Specialization:** Symmetric-Key Bootstrap & PAKE-Style Hybrid KEX  
**Perspective:** *Quantum-safety does not require running a lattice KEM on the device. A hybrid pre-shared-key + quantum-safe signature from the peer achieves equivalent security with 300 bytes of SRAM.*

---

## Core Hypothesis

The C0-PQLink architecture already has a pre-shared-key (PSK) concept and acknowledges entropy constraints. NOVA HASHBRIDGE argues that for embedded IoT sensors with a fixed commissioning phase, a **PAKE + Server-Side KEM** architecture achieves quantum-safe session establishment with far less on-device computation.

## Primary Proposal: NANO-HASP (Hash-Based Asymmetric Session Protocol)

### Threat Model for IoT Sensors

An Arduino Nano temperature sensor faces these threats:
1. **Traffic interception** — eavesdropping on sensor readings
2. **Data injection** — faking temperature data
3. **Replay attacks** — replaying old valid packets
4. **Harvest-now-decrypt-later** — store encrypted traffic, decrypt with future quantum computer

Threat 4 ("harvest now") is the primary quantum concern. Standard AES-256 already provides 128-bit post-quantum security against Grover's algorithm. **The key establishment is the attack surface.**

### NANO-HASP Architecture

```
Phase 0: COMMISSIONING (done once, in factory/lab)
─────────────────────────────────────────────────
1. Server generates ML-KEM-512 keypair (on server)
2. Server sends public key (800 bytes) to Nano over USB
3. Nano stores ek_server in EEPROM (800 bytes available in ATmega328P)
4. Server hashes ek_server → derives device_id + shared_salt (32 bytes each)
5. Nano stores device_id + shared_salt in EEPROM

Phase 1: SESSION ESTABLISHMENT (on Nano)
────────────────────────────────────────
1. Nano reads ek_server from EEPROM (800 bytes)  
   → Reads 32 bytes at a time via EEPROM callback
2. Nano samples fresh m ← random_32_bytes()
3. Nano runs ML-KEM-512 encapsulation:
   → K, c = Encaps(ek_server, m)
   → K is the session key, c is the ciphertext (768 bytes)
4. Nano sends c in fragments (authenticated against ek_hash)
5. Server decapsulates: K = Decaps(dk_server, c)
6. Both derive session key Ks = HKDF(K, shared_salt, "C0PQLINK")
7. Ascon-AEAD128 traffic begins

Phase 2: TRAFFIC (on Nano)
───────────────────────────
Nano uses Ks + Ascon + nonce counter (no more KEM needed until next session)
```

### EEPROM as Secure Key Store

ATmega328P has **1024 bytes EEPROM** — completely overlooked by prior analysis!

```
EEPROM Layout (1024 bytes total):
  [0x000 - 0x31F]  ek_server (800 bytes) — ML-KEM encapsulation key
  [0x320 - 0x33F]  shared_salt (32 bytes) 
  [0x340 - 0x34F]  device_id (16 bytes)
  [0x350 - 0x35F]  epoch_counter (16 bytes, anti-rollback)
  [0x360 - 0x3FF]  reserved (160 bytes)
```

EEPROM read: **3.3 ms per byte** (slow, but only needed at session start, not per packet)

### SRAM Budget During Encapsulation

```
Object                         Bytes
────────────────────────────── ─────
ek_server read buffer           32   ← read EEPROM 32 bytes at a time
Current NTT tile               64   ← 32 × int16_t coefficients  
Keccak state (SHAKE128)       200   ← for A matrix generation
Packed accumulator (one row)  384   ← 256 × 12-bit = 384 bytes
Encapsulation coins m          32   ← fresh random
Session key K                  32   ← output of encapsulation
Protocol context              200   ← state machine + fragment state
────────────────────────────── ─────
TOTAL                          944   ← FITS in 1024-byte crypto gate!
```

### Key Difference from RPE-32

RPE-32 tries to regenerate the public key *on every encapsulation*. NANO-HASP stores `ek_server` in EEPROM and reads it progressively. This removes the XOF seeding for A matrix expansion — the most expensive Keccak operation.

**EEPROM vs Flash:**
- EEPROM: 1024 bytes, readable in-circuit, writable (survives power loss)
- Flash PROGMEM: 32 KB, read-only at runtime, stores code + constants

### Security Properties

| Property | Value |
|----------|-------|
| Post-quantum security | 128-bit (ML-KEM-512 encapsulation is quantum-safe) |
| Forward secrecy | Yes — m is fresh random per session |
| Replay resistance | Epoch counter in EEPROM incremented per session |
| Man-in-the-middle | Prevented by commissioning step binding |
| Harvest-now-decrypt-later | Resisted — M-LWE hardness |

### Commissioning Step Security

The commissioning phase must be:
1. Performed in a trusted environment (factory or lab)
2. `ek_server` transmitted over authenticated USB/SPI (not radio)
3. Server keeps `dk_server` in HSM or secure enclave
4. `shared_salt` derived from HKDF(dk_server_hash, serial_number)

### Why This Works When Full KEM Doesn't

The bottleneck for ML-KEM on Nano is storing the **public key** for random access during encapsulation. By using EEPROM as a 800-byte cache:
- No SHAKE expansion of A needed (ek contains pre-expanded A implicitly)
- Sequential EEPROM reads suffice (access pattern is linear)
- SRAM need drops dramatically

### Honest Limitations

1. **Commissioning complexity** — requires offline key provisioning step
2. **EEPROM wear** — epoch counter write on each session (~10,000 write cycles rated; at 1 session/day = 27 years)
3. **Server key rotation** — if server re-keys, must re-commission all devices
4. **This is a hybrid** — PSK element provides pre-quantum baseline, ML-KEM provides post-quantum upgrade

---

*NOVA HASHBRIDGE — AGT-004-NHB | Research Snapshot: 2026-07-30*
