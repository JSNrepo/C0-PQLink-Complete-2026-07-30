# Migration plan for existing devices

C0-PQLink does not make immutable classical-only hardware post-quantum by
itself. The minimum prerequisite is an authorized way to install or link the
package and enough target resources to pass the hardware gates. The goal is
to avoid replacing an otherwise serviceable device when a bounded software
integration is possible.

## Separate the migration problem

Treat these as different workstreams:

| Workstream | Required action |
|---|---|
| Discovery | Inventory device model, MCU, RAM/flash, compiler, crypto use, link MTU, peer, credentials, and update path |
| Feasibility | Compile, measure stack/SRAM, benchmark latency/energy, and test packet loss on the exact target |
| Provisioning | Assign unique PSK, peer public key, 16-byte key ID, epoch, and active key slot |
| Peer readiness | Add a C0-PQLink endpoint beside the legacy endpoint |
| Policy rollout | Move cohorts through the one-way state machine |
| Operations | Rotate keys, revoke devices, observe failures, and retain a controlled recovery path |

Do not turn on a fleet-wide requirement based on a desktop benchmark.

## One-way policy states

```text
LEGACY_PSK
    |
    v
PSK_PLUS_PQ
    |
    v
PQ_REQUIRED_WITH_PSK_AUTH
```

### `LEGACY_PSK`

The existing authenticated transport is allowed. Use this only while
inventory, firmware integration, peer deployment, and measurement are in
progress.

### `PSK_PLUS_PQ`

C0-PQLink is provisioned and attempted. Fleet control can compare success,
latency, battery, packet loss, and reconnect behavior with the legacy path.
Whether a temporary fallback is allowed is an application policy outside the
session core; it must be observable, rate-limited, and time-bounded.

### `PQ_REQUIRED_WITH_PSK_AUTH`

Live traffic is accepted only after the authenticated ML-KEM handshake.
The PSK remains as the provisioning authentication factor and early DoS gate.
The state helper rejects return to a lower state.

## Authenticated A/B journal

Each storage slot is 52 bytes:

| Bytes | Field |
|---:|---|
| 4 | Magic `C0MJ` |
| 1 | Record version |
| 1 | Migration state |
| 1 | Active key slot |
| 1 | Reserved zero |
| 4 | Generation |
| 8 | Epoch |
| 16 | Key ID |
| 16 | Truncated HMAC-SHA-256 |

First provisioning uses `c0pq_migration_initialize()`, which succeeds only
when both reads explicitly report `C0PQ_JOURNAL_READ_EMPTY` and writes
generation 1 to slot 0. A negative read status is an I/O failure, not an empty
slot. Normal
updates use `c0pq_migration_commit()`, which refuses to create a new journal
when no valid current record can be loaded. This keeps corruption from being
mistaken for authorization to reset policy.

Commit writes the inactive slot, reads it back, compares the exact bytes, and
verifies its tag before considering it valid. Load selects the valid record
with the largest generation. A torn new write therefore leaves the previous
slot usable.

The 32-byte journal key must be independent of traffic keys and protected as
device configuration. The journal authenticates software state; it cannot
stop an attacker from restoring both slots and the rest of flash to an older
valid snapshot.

## Key rotation

Use two public-key slots:

1. Provision the new canonical public key in the inactive slot.
2. Compute and verify its 16-byte SHA-256 key ID.
3. Make the peer accept the new epoch and key.
4. Commit a journal record selecting the new slot with a strictly
   nondecreasing epoch.
5. Establish and test a session.
6. Retain the old private key only for the explicitly defined overlap window.
7. Retire the old key after fleet telemetry shows the target cohort moved.

The peer should enforce a minimum epoch per device so complete local-storage
rollback cannot silently re-enable an old key.

## Cohort rollout

Use small cohorts ordered by operational recoverability:

1. development boards;
2. lab devices with power and packet-loss injection;
3. internal noncritical devices;
4. a small field canary;
5. progressively larger cohorts;
6. critical devices only after target evidence and independent review.

Track at least:

- handshake success and failure reason;
- retries by frame type and fragment index;
- time and energy per connection;
- peak SRAM/stack;
- reconnect frequency;
- record authentication/replay failures;
- epoch and migration state;
- battery or energy-harvest budget impact.

Never log PSKs, ML-KEM randomness/shared secrets, chain keys, plaintext, or the
peer private seed.

## Modes as an operational choice

`FULL_PQ_EACH_SESSION` gives a fresh ML-KEM exchange on every new session. It
is the simpler security story but costs a full handshake after every
disconnect.

`PQ_BOOTSTRAP_RATCHET` is suitable when one established session carries many
records and repeated public-key computation is the dominant cost. It still
starts with ML-KEM and should periodically reconnect according to a measured
policy. It is not a substitute for key rotation or post-compromise healing.

## Legacy integration boundary

Adapters isolate existing application code from the cryptographic package:

- public-key reads map to existing flash/secure-element APIs;
- RNG maps to a hardware or approved DRBG API;
- transport maps to LoRa, NB-IoT, UART modem, 802.15.4, BLE, or another
  packet service;
- application payloads remain at most 48 bytes per protected record.

The reference peer can sit beside an existing endpoint during evaluation. It
is not a permanent cloud dependency and must not receive device-side
cryptographic operations for execution.

## Stop conditions

Do not advance a cohort if any of these remain unknown:

- true worst-case SRAM and stack margin;
- RNG security;
- handshake completion inside the application deadline;
- energy impact inside the service-life budget;
- recovery after power loss during migration writes;
- loss behavior on the actual link;
- key revocation and peer rollback behavior;
- target side-channel/fault exposure;
- accountable owner for future algorithm or protocol updates.
