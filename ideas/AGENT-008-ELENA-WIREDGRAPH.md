# Agent 008 — ELENA WIREDGRAPH
**ID:** AGT-008-EWG  
**Specialization:** Network Protocol Optimization & Fragment-Level Cryptographic Offloading  
**Perspective:** *If the Nano can't do the full KEM in one shot, the protocol should let the peer do the heavy lifting — with cryptographic guarantees.*

---

## Core Hypothesis

The current C0-PQLink protocol has the Nano as CLIENT (encapsulator). ELENA WIREDGRAPH challenges this: in many IoT deployments, the **resource-rich peer initiates** and the device responds. By making the Nano the RESPONDER (decapsulator's client), we shift 80% of KEM computation to the peer.

## Primary Proposal: INVERSE-KEM Protocol

### Standard KEM Flow (Current)

```
Nano (encapsulator):                  Peer (decapsulator):
- Has peer's public key (800 bytes)   - Has private key
- Runs encapsulation                  - Runs decapsulation
- Sends ciphertext (768 bytes)        - Recovers shared secret
- Both derive session key             - Both derive session key
```

**Problem:** Encapsulation on Nano = NTT multiply + polynomial arithmetic.

### INVERSE-KEM Flow (Proposed)

```
Nano (decapsulator):                  Peer (encapsulator):
- Has own public key stored in flash  - Has Nano's public key (fetched at setup)
- Receives ciphertext (768 bytes)     - Runs encapsulation (on powerful peer)
- Runs decapsulation                  - Sends ciphertext to Nano
- Recovers session key                - Both derive session key
```

**ML-KEM decapsulation on the Nano** requires:
1. Receive 768-byte ciphertext `c` (fragment by fragment)
2. Compute `(u, v) = decode(c)` — just bitwise operations
3. Compute `K' = innerProduct(sk_hat, u) = sk · u` in NTT domain
4. Compute `w = v - K' - encode(H_G_output)` — one poly subtraction
5. `m' = decode(w)` — one polynomial
6. Re-encapsulate to verify: `K'' = G(m' || H(pk))`
7. Accept K if K' == K''

### Decapsulation Memory Analysis

```
Object                        Bytes  Notes
───────────────────────────── ─────  ─────────────────────────────
sk_hat (NTT-domain secret)    512    MUST be stored! (no shortcut)
Received ciphertext (u)       640    Stored progressively
Received ciphertext (v)       128    Remaining part
Re-encaps working memory      400    Similar to encapsulation
Keccak state                  200    
Verification K', K''           64   
Protocol + fragment state     200   
───────────────────────────── ─────
TOTAL                        2144    ← Still too large
```

**Decapsulation is ALSO too large** — the NTT-domain private key `sk_hat` is 512 bytes and cannot be compressed or regenerated.

### Modified Approach: FRAGMENT-DECAPS

Break the decapsulation into small operations:

**Step 1: Receive ciphertext in fragments (no SRAM for full 768 bytes)**
```c
// Don't store full ciphertext — process each fragment immediately
// u has 320 bytes for row 0, 320 bytes for row 1
// Process each row as it arrives:
while (receive_fragment(&frag)) {
    // decompress one u-row (160 byte compressed → 320 decoded)
    decompress_u_row(frag.data, u_row, row_idx);
    // Compute inner product with one row of sk_hat
    ntt_multiply_accumulate(sk_hat_row, u_row, accumulator);
}
```

**SRAM with FRAGMENT-DECAPS:**
```
sk_hat (two rows, NTT-domain): 512 bytes  ← unavoidable
One u-row (working):           320 bytes  ← per row, not both at once
Keccak state:                  200 bytes
Other state:                   200 bytes
────────────────────────────── ─────────
TOTAL:                        1232 bytes  ← Fits! (< 1792 bytes)
```

**This is the key insight:** Store `sk_hat` (private key) in SRAM permanently (512 bytes), receive ciphertext fragments and process one at a time, never storing the full 768-byte ciphertext.

### sk_hat Persistence Strategy

The Nano's private key `dk_nano` (the NTT-domain form, 512 bytes) can be:
- **Generated once** during commissioning
- **Stored in EEPROM** (1024 bytes available, 512 bytes for sk_hat)
- **Loaded into SRAM** at session start (512 bytes — fits within budget)
- **Kept in SRAM** during the active session

This is mathematically sound: `dk_nano` is the device's long-term private key, not a session secret.

### NANO Decapsulator Complete Protocol

```
COMMISSIONING (once):
  1. Generate ML-KEM-512 keypair: (ek_nano, dk_nano) — done on peer, offline
  2. Store dk_nano (64+512 = 576 bytes) in EEPROM
  3. Register ek_nano (800 bytes) with peer server

SESSION ESTABLISHMENT:
  1. Nano powers on, loads dk_nano from EEPROM to SRAM (512 bytes)
  2. Nano sends HELLO (device_id, epoch, nonce_nano)
  3. Peer generates fresh m, runs Encaps(ek_nano, m) → (K, c)
  4. Peer sends c in 32-byte authenticated fragments
  5. Nano receives fragments, processes each immediately:
     - Decompress fragment of u
     - Accumulate into sk_hat dot product
  6. After all fragments received: compute session key K
  7. Both verify shared secret via Finished exchange
  8. Ascon traffic begins

TRAFFIC:
  - Nano keeps session key Ks (32 bytes) in SRAM
  - dk_nano remains in SRAM for next re-key
```

### Security Properties of INVERSE-KEM

| Property | Status | Notes |
|----------|--------|-------|
| Post-quantum secrecy | ✅ | ML-KEM-512 security |
| Forward secrecy | ✅ | m is fresh per session |
| Device authentication | ✅ | Only owner of dk_nano can decapsulate |
| Peer authentication | ⚠️ | Peer must prove knowledge of ek_nano hash |
| Replay resistance | ✅ | Epoch counter |

### Trade-Off vs Nano Encapsulation

| | Nano Encapsulates | Nano Decapsulates |
|--|------------------|------------------|
| Nano SRAM | ~950 bytes (RPE-32) | **1232 bytes** |
| Nano computation | NTT multiply | NTT multiply (same!) |
| Peer computation | NTT multiply | NTT multiply (same!) |
| Key storage | Peer's ek (EEPROM) | Own dk (EEPROM) |
| Session initiation | Nano initiates | Peer initiates |

**Conclusion:** INVERSE-KEM doesn't significantly reduce computation, but it changes what's stored in SRAM vs EEPROM, enabling a cleaner memory layout.

---

*ELENA WIREDGRAPH — AGT-008-EWG | Research Snapshot: 2026-07-30*
