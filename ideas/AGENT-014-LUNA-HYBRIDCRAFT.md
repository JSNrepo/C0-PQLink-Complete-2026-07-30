# Agent 014 — LUNA HYBRIDCRAFT
**ID:** AGT-014-LHC  
**Specialization:** Hybrid Classical-PQ Protocol Design & Layered Security  
**Perspective:** *Don't replace classical crypto. Stack quantum-safe on top of X25519. Perfect forward secrecy + post-quantum hardness from two independent assumptions.*

---

## Core Hypothesis

The gold standard of post-quantum security is not replacing classical algorithms — it's a **hybrid**: X25519 (or X448) for classical forward secrecy + a post-quantum KEM for harvest-now-decrypt-later resistance. LUNA HYBRIDCRAFT designs a hybrid that fits the ATmega328P by leveraging the extreme efficiency of X25519 on AVR.

## Algorithm Proposal: NANO-HYBRID (X25519 + NANO-RLWE Hybrid KEM)

### Why X25519 Fits on Nano

X25519 (Curve25519 Diffie-Hellman):
- **Key size:** 32-byte public key, 32-byte private key
- **Output:** 32-byte shared secret
- **AVR implementation:** ~1-2 MB flash, ~230 byte SRAM, ~1 second at 16 MHz
- **IANA RFC 7748:** standardized, widely deployed
- **Post-quantum status:** BROKEN by Shor's algorithm (but still 128-bit classical)

NANO-RLWE (from Agent 009):
- **Public key:** 112 bytes
- **Ciphertext:** 112 bytes  
- **SRAM:** 766 bytes
- **Post-quantum security:** ~130-bit quantum
- **Time:** ~25 ms at 16 MHz

### Combined Hybrid Protocol

```
SESSION ESTABLISHMENT:

Step 1: Nano generates ephemeral X25519 keypair
  ek_x (32B), dk_x (32B) — total 64 bytes
  
Step 2: Nano sends HELLO { device_id, epoch, ek_x (32B), nonce_nano (32B) }
  = 96 bytes per frame, within 802.15.4 budget

Step 3: Peer runs X25519: DH_classical = X25519(peer_dk_x, ek_x)
  Peer sends CHALLENGE { ek_peer_x (32B), pq_ciphertext (112B), mac (16B) }
  = 160 bytes → 2 authenticated fragments

Step 4: Nano:
  a. DH_classical = X25519(dk_x, ek_peer_x)         ← classical key
  b. K_pq = RLWE_Decaps(dk_rlwe, pq_ciphertext)     ← PQ key
     Wait — Nano doesn't have dk_rlwe... need RLWE keypair on Nano!

Alternative: Nano ENCAPSULATES to peer's RLWE public key:
  a. DH_classical = X25519(dk_x, ek_peer_x)
  b. (K_pq, pq_ct) = RLWE_Encaps(ek_peer_rlwe_nano)  ← PQ encapsulation
  c. K_session = HKDF(DH_classical || K_pq, nonce_nano || nonce_peer)
  d. Nano sends pq_ct (112 bytes) → 2 fragments
  e. Both: K_session = HKDF(DH_classical || K_pq, ...)
```

### Revised Protocol with Nano Encapsulating

```
Nano                                    Peer
 │ HELLO: device_id, epoch, ek_x, nonce ─────────────────────────────────►│
 │                                       Peer receives, sends ek_peer_rlwe  │
 │◄─── CHALLENGE: ek_peer_x, ek_peer_rlwe_nano, tag ──────────────────────│
 │ Nano computes:                        │
 │   DH = X25519(dk_x, ek_peer_x)       │
 │   (K_pq, c) = RLWE_Encaps(ek_peer_rlwe)
 │   K_session = HKDF(DH || K_pq, ...)  │
 │──── RESPONSE: ek_x, c, finished_mac ──────────────────────────────────►│
 │                                       Peer: K_pq = RLWE_Decaps(dk_rlwe, c)
 │                                       K_session = HKDF(DH || K_pq, ...)
 │◄─── FINISHED: finished_mac ───────────────────────────────────────────│
 │ Ascon traffic begins                  │
```

### SRAM Budget for NANO-HYBRID

```
Object                          Bytes  Notes
─────────────────────────────── ─────  ─────────────────────────────
X25519 ephemeral keypair          64   ek_x (32) + dk_x (32)
X25519 scalar multiply state      80   Curve25519 ladder state
NANO-RLWE encapsulation:
  Keccak state                   200
  Secret r (ternary packed)       16   64 coefficients, 2-bit packed
  Accumulator (n=64, 10-bit pk)   80
  Tile buffer                     64
HKDF working state               96   SHA256 × 2 for HKDF
DH output (shared secret)        32
Protocol + session state         200
─────────────────────────────── ─────
TOTAL                            832   ← Under 1024-byte crypto gate!
```

**Key insight:** X25519 and NANO-RLWE share the Keccak/SHA256 hardware — sequential use, single 200-byte state.

### Security Properties

```
Attack type                  Defender        Protection
─────────────────────────── ────────────── ─────────────────────────
Classical eavesdrop          X25519 DH       ✅ 128-bit classical
Shor's algorithm breaks DH   NANO-RLWE KEM   ✅ ~130-bit PQ security
Grover's on session key      AES-256/Ascon   ✅ 128-bit PQ
Compromise of RLWE only      X25519 DH       ✅ Session still protected
Compromise of DH only        NANO-RLWE       ✅ Session still protected
Both DH + RLWE broken        —               ❌ Session compromised
```

**This is the definition of hybrid post-quantum security**: an attacker must break **BOTH** classical DH AND lattice assumptions to recover the session key. Breaking one (even with a quantum computer) leaves the other standing.

### Comparison to TLS 1.3 Post-Quantum Hybrid

This protocol mirrors RFC 8446 (TLS 1.3) combined with NIST PQC hybrid drafts (e.g., draft-ietf-tls-hybrid-design):
- TLS 1.3: X25519 + ML-KEM-768 (IANA registered)
- NANO-HYBRID: X25519 + NANO-RLWE (custom, experimental)
- The difference is parameter size — same security architecture

### X25519 AVR Implementation Reference

**TweetNaCl** (Daniel J. Bernstein's minimal crypto library):
- 25519 implementation: ~400 bytes code, ~230 bytes SRAM
- License: public domain
- Port for AVR: available, ~1-1.5 seconds at 16 MHz
- Reference: `tweetnacl-avr` by Kaspar Schleiser (MIT)

### Latency Analysis

| Operation | Time at 16 MHz |
|-----------|---------------|
| X25519 keypair gen | ~0.8 seconds |
| X25519 DH | ~0.8 seconds |
| NANO-RLWE encapsulation | ~25 ms |
| HKDF-SHA256 | ~15 ms |
| **Total session establishment** | **~1.7 seconds** |

For IoT sensor applications: 1.7 seconds is fully acceptable (session typically lasts hours).

### Implementation Plan

1. Port TweetNaCl X25519 to AVR (or use existing `avr-x25519` from Kaspar Schleiser)
2. Implement NANO-RLWE (Agent 009's design)
3. Combine in `src/session/hybrid_kem.c`
4. HKDF combiner: `K = HKDF(DH_secret || RLWE_secret, nonces, "NANO-HYBRID-v1")`
5. Integration test: full session with hybrid establishment
6. Measure SRAM peak on `simavr` ATmega328P simulation

---

*LUNA HYBRIDCRAFT — AGT-014-LHC | Research Snapshot: 2026-07-30*
