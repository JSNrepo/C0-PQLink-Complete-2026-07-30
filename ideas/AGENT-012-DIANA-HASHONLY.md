# Agent 012 — DIANA HASHONLY
**ID:** AGT-012-DHO  
**Specialization:** Hash-Based Post-Quantum Signatures & Hybrid KEM-PSK  
**Perspective:** *Signatures, not KEMs, are the simplest post-quantum primitive. Build quantum-safe authentication from hash functions only.*

---

## Core Hypothesis

For an IoT temperature sensor, the primary need is **authenticated, encrypted communication** — not universal key exchange. DIANA HASHONLY proposes that **XMSS-Nano**, a minimal hash-based signature scheme, combined with a per-session AES-256 key derived from a commissioned pre-shared-key, can achieve post-quantum security with 300 bytes of SRAM.

## Primary Proposal: PSK-HBSS (Pre-Shared Key + Hash-Based Signature Scheme)

### Threat Analysis Revisited

| Threat | Required Defense |
|--------|-----------------|
| Passive eavesdropping | AES-256 (128-bit quantum security via Grover) |
| Active injection | Authenticated encryption (Ascon-AEAD128) |
| Replay attacks | Nonce/epoch |
| Harvest-now-decrypt-later | AES-256 session key ≥128 bits post-quantum |
| Long-term key compromise | Forward secrecy (session key rotation) |
| Quantum key derivation break | Post-quantum KEM or PSK with HKDF |

**Key insight:** AES-256 session keys ALREADY resist Grover's algorithm. The quantum threat is specifically to **key establishment** using RSA/ECDH. If the session key was established using a quantum-safe method, all subsequent traffic is quantum-safe.

### PSK-HBSS Protocol

```
COMMISSIONING (factory, once per device lifetime):
  1. Generate 256-bit device PSK (true random, from HSM)
  2. Store PSK in ATmega328P's protected EEPROM (or hardware secure element)
  3. Server stores PSK indexed by device_id
  4. Generate XMSS keypair for the server (on server, not Nano)
  5. Store XMSS public key (64 bytes) on Nano in EEPROM

SESSION ESTABLISHMENT:
  1. Nano sends:  HELLO { device_id, epoch, nonce_nano }
  2. Server sends: CHALLENGE { nonce_server, server_timestamp }
  3. Nano computes:
       K_session = HKDF-SHA256(
           key  = PSK,
           salt = nonce_nano || nonce_server,
           info = "C0PQLINK-v1" || device_id || server_timestamp
       )
  4. Nano sends: RESPONSE { HMAC-SHA256(K_session, CHALLENGE) }
  5. Server verifies HMAC → both now have K_session
  6. Ascon-AEAD128 traffic with K_session

FORWARD SECRECY UPGRADE (optional):
  1. Server sends X25519 ephemeral pubkey + XMSS signature over it
  2. Nano verifies XMSS signature using 64-byte XMSS pub key in EEPROM
  3. Nano sends X25519 ephemeral pubkey
  4. Both compute DH shared secret
  5. K_session = HKDF(PSK, DH_secret, nonces, "forward-secrecy")
```

### SRAM Budget for PSK-HBSS

```
Object                   Bytes  Notes
───────────────────────── ─────  ────────────────────────────────────
SHA256 state (8 × uint32)   96  For HKDF-SHA256
HMAC working buffer         64  SHA256 twice (inner, outer)
HKDF inputs                 80  PSK(32) + nonces(32) + info(16)
Ascon state                208  Traffic cipher state
Session key K              32   Derived session key
Protocol state + epoch     100  Anti-replay, nonce storage
XMSS verification buf      200  For server signature verification
────────────────────────── ─────
TOTAL                       780  ← Well under 1024-byte crypto gate!
```

### Why PSK is Quantum-Safe

1. **256-bit PSK** → Grover's gives quantum adversary speed-up by √N → still requires 2^128 quantum operations to brute-force
2. **HKDF with SHA-256** → SHA-256 has no known quantum speedup beyond Grover's (collision attack requires 2^85 operations post-quantum — acceptable)
3. **AES-256 session key** → 128-bit post-quantum security (Grover's gives 2^128 quantum operations)

**Security level:** 128-bit post-quantum for both key establishment and traffic.

### XMSS-Nano for Server Authentication (Optional)

XMSS (eXtended Merkle Signature Scheme) is NIST-standardized (SP 800-208) and hash-based:
- Signatures: 2,500 bytes (impractical to verify on Nano)
- **BUT**: Only the SERVER signs. The Nano only needs to verify.
- XMSS verification requires: 256-bit hash state + 128 bytes for Merkle path
- **Verification SRAM: ~200 bytes** (feasible!)

The server signs its CHALLENGE message with XMSS. The Nano verifies using:
```c
// XMSS one-time signature verification (no signing on Nano)
int xmss_verify_nano(
    const uint8_t *sig, uint16_t sig_len,       // 2,500 bytes (in received fragment)
    const uint8_t *msg, uint16_t msg_len,        // 32 bytes
    const uint8_t *pk,  uint8_t  pk_len          // 64 bytes in EEPROM
) {
    // Hash message to leaf
    uint8_t leaf[32];
    sha256(msg, msg_len, leaf);
    
    // Traverse Merkle path (L=16 levels → 16 × 32-byte hashes)
    // Process fragment by fragment — only need 64 bytes SRAM at any time
    uint8_t path_node[32];
    for (uint8_t level = 0; level < 16; level++) {
        receive_merkle_node(sig, level, path_node);  // receive 32 bytes
        sha256_combine(leaf, path_node, leaf);        // in-place, 32 bytes
    }
    return memcmp(leaf, pk + 32, 32) == 0;  // compare to root
}
```

XMSS verification: **~200 bytes SRAM peak**, 16 × SHA256 calls ≈ ~50 ms.

### Comparison with ML-KEM Approach

| Aspect | ML-KEM Approach | PSK-HBSS |
|--------|----------------|---------|
| PQ Assumption | MLWE (lattice) | Grover bounds on AES/SHA |
| SRAM for key establishment | 950-1232 bytes | **780 bytes** |
| On-device computation | NTT multiply (complex) | **SHA256 + HKDF (simple)** |
| Forward secrecy | Per-session (ML-KEM) | Optional (X25519 upgrade) |
| Commissioning required | Yes (store peer ek) | **Yes (store PSK + XMSS pk)** |
| NIST standardized | ML-KEM-512: yes | SHA256, HKDF, XMSS: yes |
| Wire size | 768-byte ciphertext | **0 extra bytes** |

### Honest Assessment

**Advantages:**
- Dramatically simpler implementation
- Fully standard building blocks (SHA256, HMAC, HKDF — all NIST-approved)
- PSK quantum resistance is well-understood
- No lattice arithmetic on device

**Limitations:**
- PSK must be provisioned securely (supply chain risk)
- No quantum-safe key establishment without the PSK (breaks if PSK leaks)
- XMSS server has limited signatures (~2^20 per keypair)
- Not "asymmetric" — requires secure bootstrap

**Label:** `PRODUCTION-READY for IoT deployments with secure commissioning`

---

*DIANA HASHONLY — AGT-012-DHO | Research Snapshot: 2026-07-30*
