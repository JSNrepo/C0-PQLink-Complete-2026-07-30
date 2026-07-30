# Agent 006 — PRIYA MULTIPHASE
**ID:** AGT-006-PMP  
**Specialization:** Protocol-Level Phasing & Multi-Session Pre-computation  
**Perspective:** *A single ML-KEM encapsulation doesn't have to complete in one power cycle. Design the protocol to survive interruptions and amortize expensive operations across time.*

---

## Core Hypothesis

The 2 KB SRAM constraint is severe when attempting full encapsulation atomically. PRIYA MULTIPHASE proposes **temporal decomposition**: splitting the ML-KEM encapsulation into 4 phases that execute across multiple Arduino `loop()` iterations, with intermediate state checkpointed to EEPROM.

## Algorithm Proposal: TEMPO-KEM (Temporal Encapsulation with Phased Memory Operations)

### Why Temporal Decomposition Works

Standard ML-KEM encapsulation must produce:
- `u = A·s + e1` (two polynomial multiplications + error add)
- `v = ek_t·s + e2 + ⌈q/2⌉·m` (inner product + error + message)

These operations are mathematically independent across rows of A. The order of operations is:
1. Row 0 of u: A[0][0]·s[0] + A[0][1]·s[1] + e1[0]
2. Row 1 of u: A[1][0]·s[0] + A[1][1]·s[1] + e1[1]  
3. v: t[0]·s[0] + t[1]·s[1] + e2 + message
4. Final hashing and output

**None of these phases requires the results of the previous phase in SRAM simultaneously.**

### Phase Architecture

```
Phase 0 (Initialization) — ~50 ms
──────────────────────────────────
- Sample fresh m (32 bytes)
- Compute K̄ = G(m ∥ H(ek))
- Store: m (32B), K' (32B), r (32B), ρ (32B) → 128 bytes to EEPROM checkpoint

Phase 1 (Row 0 of u) — ~80 ms  
───────────────────────────────
- Load r, ρ from EEPROM
- Compute u_0 = A[0][0]·s[0] + A[0][1]·s[1] + e1[0]
- Stream 320 bytes of u_0 to fragment buffer
- No intermediate state kept in SRAM after phase
- Write fragment 0 to peer → get ACK → continue

Phase 2 (Row 1 of u) — ~80 ms
───────────────────────────────
- Load r, ρ from EEPROM
- Compute u_1 = A[1][0]·s[0] + A[1][1]·s[1] + e1[1]
- Stream 320 bytes of u_1 to fragment buffer
- Write fragment 1 to peer → get ACK

Phase 3 (v component) — ~60 ms  
───────────────────────────────
- Load r, ρ, m from EEPROM
- Compute v = t·s + e2 + encode(m)
- Stream 128 bytes of v to fragment buffer
- Write fragment 2 to peer → get ACK

Phase 4 (Finalization) — ~20 ms
────────────────────────────────
- Load K' from EEPROM
- Verify ACK from peer (peer decapsulated successfully)
- Derive session key Ks = KDF(K', "session")
- Erase EEPROM checkpoint
- Begin Ascon traffic
```

### SRAM Budget Per Phase (Maximum)

```
Phase 1 (largest):
  Keccak state (XOF)            200
  Secret tile (32 int16_t)       64
  A-matrix tile (32 int16_t)     64
  Accumulator (12-bit packed)   384
  r + ρ (seeds)                  64
  Protocol + fragment state     150
  ─────────────────────────────────
  PEAK SRAM:                    926   ← Under 1024-byte crypto gate ✓
```

### EEPROM Checkpoint Layout

```
EEPROM Region [0x000 - 0x0BF] (192 bytes):
  [0x00-0x1F]  r    (32 bytes) — ephemeral coins
  [0x20-0x3F]  ρ    (32 bytes) — public matrix seed (from ek footer)
  [0x40-0x5F]  m    (32 bytes) — encapsulation message
  [0x60-0x7F]  K'   (32 bytes) — intermediate session key
  [0x80-0x8F]  phase_id (1 byte) + status flags
  [0x81-0xBF]  reserved / fragment ACK state
```

EEPROM write: ~3.3 ms per byte. 192-byte checkpoint write: ~630 ms once per session. **Acceptable** — session establishment is a one-time cost.

### Protocol Timeline

```
     Nano                            Peer
      │  Phase 0: init + checkpoint  │
      │  Phase 1: compute u_0        │
      │──── fragment 0 (u_0) ───────►│
      │◄─── ACK ────────────────────│
      │  Phase 2: compute u_1        │
      │──── fragment 1 (u_1) ───────►│
      │◄─── ACK ────────────────────│
      │  Phase 3: compute v          │
      │──── fragment 2 (v) ─────────►│
      │◄─── ACK + Finished ─────────│
      │  Phase 4: finalize           │
      │──── Ascon traffic ──────────►│
```

### Fault Tolerance Built In

If the Nano powers off mid-session:
1. On reboot, read EEPROM checkpoint
2. If `phase_id` ≠ COMPLETED: restart from last phase
3. Fragment ACKs are stored in EEPROM — no duplicate transmission
4. If EEPROM shows COMPLETED: erase and start fresh session

### Power Consumption Analysis

At 16 MHz active, ATmega328P draws ~12 mA. Each phase takes ~80 ms:
- Total encapsulation: ~290 ms active
- Energy: 12 mA × 290 ms = 3.48 mJ per session establishment
- If session lasts 1 hour: 3.48 mJ / (3600s × 1000ms/s) = **negligible**

### Integration with Existing C0-PQLink

Phase transitions map cleanly to the existing state machine:
```c
typedef enum {
    SESSION_PHASE_INIT    = 0,
    SESSION_PHASE_U0      = 1,
    SESSION_PHASE_U1      = 2,
    SESSION_PHASE_V       = 3,
    SESSION_PHASE_FINAL   = 4,
    SESSION_PHASE_TRAFFIC = 5
} session_phase_t;
```

The existing `c0pq_client_context` struct gains a `phase` field and an EEPROM address for checkpoint.

### What This Unlocks

By amortizing encapsulation across loop iterations:
- Each phase uses <950 bytes SRAM
- Nano is responsive between phases (can read sensors, blink LEDs)
- Protocol can handle peer timeouts per-phase (not just at the end)
- EEPROM checkpointing provides hardware-level fault tolerance

---

*PRIYA MULTIPHASE — AGT-006-PMP | Research Snapshot: 2026-07-30*
