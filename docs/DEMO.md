# Hackathon demonstration

## Opening statement

> C0-PQLink protects live IoT connections. It is not secure boot, firmware
> update, cloud-offloaded crypto, or a new PQ algorithm. The constrained
> client performs exact ML-KEM-512 encapsulation locally, sends its 768-byte
> ciphertext as authenticated 48-byte fragments, verifies the opposite
> endpoint, and then exchanges Ascon-protected sensor records.

## Prerequisites

- C99/C++11 compiler and Make;
- Node.js 20.19 or newer;
- npm dependencies installed with `npm ci`.

The automated demo does not require a radio or cloud service.

## Reproducible evidence run

```sh
npm ci
make clean
make verify
make size-report
```

The decisive lines are:

```text
ML-KEM-512 oracle: 8/8 exact vectors + canonical-key rejection passed
Live-session interop: ML-KEM, Finished, Ascon ratchet, tamper rejection, and handshake/data loss recovery passed
Live-session FULL_PQ_EACH_SESSION interoperability passed
C0-PQLink: all tests passed
mlkem_workspace_internal=1312
mlkem_workspace_public_bound=1344
frame_max=96
record_plaintext_max=48
```

Explain the interop test while it runs:

1. A C device process receives only the peer public key, PSK, key ID, and
   transport adapter.
2. The C code produces exact ML-KEM bytes.
3. An independent JavaScript implementation decapsulates with the private
   key.
4. The test deliberately drops the first Challenge response, fragment-7 ACK,
   Peer Finished response, and protected application response.
5. It also injects a forged fragment, Finished, and data record.
6. An exact sealed-request retry receives the peer's cached response without
   repeating the application action.
7. The legitimate session still establishes and exchanges
   `temperature=24.5` / `ack:temperature=24.5`.

## Show the import surfaces

Arduino/C++:

```cpp
#include <C0PQLink.h>

C0PQLink::Client client;
C0PQLink::Workspace workspace;
client.begin(
    deviceId, psk, epoch, peerKeyId,
    flashPublicKey, secureRandom, packetTransport
);
client.connect(workspace);
```

MicroPython:

```python
from c0pqlink import Client

link = Client(
    device_id, psk, epoch, peer_key_id,
    flash_public_key_reader, packet_transport,
    rng=board_secure_random,
).connect()
```

Make the implementation boundary explicit: developers provide flash reads,
secure randomness, and packet I/O; they do not implement ML-KEM, HKDF,
fragment authentication, Finished, Ascon, replay state, or ratcheting.

## Optional UDP peer

Generate paired demo material:

```sh
npm run provision:demo
cp generated/reference-peer-config.json reference-peer/config.json
npm run peer
```

The generated header contains public/device provisioning. The peer JSON also
contains the ML-KEM private seed and PSK; keep it private and never put it on
the device.

The UDP peer demonstrates replaceability, not cloud offload. A board demo
still requires a transport adapter that exchanges one C0-PQLink frame per
datagram/packet.

## Four-minute pitch flow

### 0:00–0:30 — Problem and scope

Show the architecture image. State that standardized PQC availability is not
the same as deployability on existing constrained fleets. Lock the scope to
live connections.

### 0:30–1:15 — Technical contribution

Show the callback-backed 800-byte public key, 1,344-byte current workspace,
exact ML-KEM output, five-byte primitive writer calls, and 48-byte network
fragments. Then show the linked Nano rejection: 2,016 static SRAM bytes before
stack. Emphasize that the math is unchanged and the RPE-32 replacement remains
a measured hypothesis, not a completed fit claim.

### 1:15–2:10 — Working connection

Run `make interop-test`. Describe private-key decapsulation as the normal peer
role. Point out injected handshake/data loss and tampering.

### 2:10–2:50 — Developer adoption

Show the C++ and MicroPython imports and three adapters. Explain that the
reference peer can be replaced by the deployed gateway or controller.

### 2:50–3:25 — Migration

Show:

```text
LEGACY_PSK -> PSK_PLUS_PQ -> PQ_REQUIRED_WITH_PSK_AUTH
```

Explain key IDs/epochs, two key slots, authenticated A/B storage, and cohort
rollout.

### 3:25–4:00 — Evidence and honesty

Show `docs/CLAIM-LEDGER.md`. State what passes and what is still required:
linked AVR disassembly, named-board stack/energy/network measurements,
MicroPython port build, fuzzing, and independent audit.

## Likely questions

**Did you create a weaker lightweight PQ algorithm?**  
No. The device output is exact ML-KEM-512. We changed execution order,
storage, and transport.

**Why not just use Ascon?**  
Ascon protects traffic after a key exists; it does not provide public-key key
establishment. C0-PQLink combines it with ML-KEM.

**Why not store a 32-byte ML-KEM private seed?**  
The constrained client is the encapsulator and owns no ML-KEM private key.
Its challenge is reading the peer public key and computing ciphertext within
RAM.

**Why is there Node code?**  
It independently proves interoperability and acts as the replaceable
private-key endpoint. No device computation is delegated to it.

**Does it run on an Arduino Nano today?**  
The host tests and constrained memory structure pass; a named AVR linked
build, stack high-water, timing, and energy run are still mandatory. Do not
claim the hardware gate before producing it.

**Is C0-PQLink standardized?**  
No. ML-KEM-512 and Ascon-AEAD128 are standardized. The protocol is
experimental and needs review.

## Demo failure policy

- If an oracle byte differs, stop: do not continue with a security claim.
- If the AVR audit is skipped, label it pending rather than passed.
- If a board lacks a documented secure RNG, do not substitute weak entropy.
- If the radio cannot preserve one frame per packet, fix the adapter before
  presenting field interoperability.
- Never use real fleet secrets in a demo.
