# C0-PQLink protocol v1

Status: experimental specification for interoperable testing. The primitive
algorithms are standardized; this composition and wire protocol are not.

## Roles

- The constrained **client** is the ML-KEM encapsulator.
- The key-owning **peer** is the ML-KEM decapsulator.
- Each client and peer share a unique 32-byte provisioning PSK.
- The client pins the peer's canonical 800-byte ML-KEM-512 public key, a
  16-byte key ID, and an unsigned 64-bit epoch.

The C identifiers use `server_*` for the peer-to-client direction. That is a
directional protocol role, not a requirement for a cloud service.

No public-key operation occurs during client initialization or application
record processing. ML-KEM runs only after the application explicitly requests
a connection and the PSK-authenticated Challenge passes verification.

## Encoding

All multibyte integers are unsigned and big-endian. Byte ranges below are
inclusive. A receiver rejects a frame whose physical length is not exactly
`10 + payload_length`.

### Common header

| Offset | Bytes | Field | Required value |
|---:|---:|---|---|
| 0 | 2 | Magic | ASCII `C0` |
| 2 | 1 | Wire version | `0x01` |
| 3 | 1 | Frame type | Registry below |
| 4 | 1 | Flags | Type-specific |
| 5 | 1 | Payload length | 0–255 |
| 6 | 4 | Session ID | Client-selected |

The 32-bit session ID is a demultiplexing value, not a secret and not the sole
replay defense.

### Frame registry

| Type | Name | Flags | Total bytes |
|---:|---|---:|---:|
| 1 | Hello | 0 | 60 |
| 2 | Challenge | 0 | 84 |
| 3 | Ciphertext Fragment | 0 | 29 + fragment length; normally 77 |
| 4 | Ciphertext ACK | 0 | 28 |
| 5 | Device Finished | 0 | 26 |
| 6 | Peer Finished | 0 | 26 |
| 7 | Data | 0 client→peer, 1 peer→client | 34 + plaintext length |
| 127 | Error | Reserved | Not emitted by v0.1.0 |

## Cryptographic suite

Suite `0x0001` is:

- ML-KEM-512 from FIPS 203;
- HKDF-SHA-256 from RFC 5869;
- HMAC-SHA-256 truncated to 16 bytes for handshake authentication;
- Ascon-AEAD128 from NIST SP 800-232 with a full 16-byte tag.

No ML-KEM parameter, distribution, transform, encoding, ciphertext, or shared
secret is modified by C0-PQLink.

## Authentication notation

`HMAC16(K, label, data)` means the first 16 bytes of:

```text
HMAC-SHA-256(K, UTF8(label) || data)
```

Labels have no trailing NUL unless a formula explicitly shows `0x00`.

The provisioning key schedule begins with:

```text
early_secret = HKDF-Extract(32 zero bytes, PSK)
hello_key    = HKDF-Expand(early_secret, "C0PQ/1 hello key", 32)
```

## Handshake

### 1. Authenticated Hello

```text
header(10)
suite(2)
device_id(8)
epoch(8)
device_nonce(16)
tag(16)
```

The 44-byte core is `header` through `device_nonce`.

```text
tag = HMAC16(hello_key, "C0PQ/1 hello frame", hello_core)
```

`device_nonce` must contain 128 bits from the configured cryptographic random
source. The implementation has no weak fallback.

### 2. Authenticated Challenge

```text
header(10)
suite(2)
epoch(8)
key_id(16)
device_nonce(16)
peer_nonce(16)
tag(16)
```

`key_id` is the first 16 bytes of SHA-256 over the canonical 800-byte
ML-KEM-512 public key. The peer echoes the device nonce and supplies a fresh
128-bit peer nonce.

```text
session_auth_key =
    HMAC-SHA-256(
        early_secret,
        "C0PQ/1 session auth" ||
        suite || epoch || key_id || device_nonce || peer_nonce
    )

tag = HMAC16(
    session_auth_key,
    "C0PQ/1 challenge frame",
    challenge_core
)
```

The client rejects a bad tag, wrong suite, wrong epoch, wrong key ID, wrong
echoed nonce, session mismatch, flags mismatch, or length mismatch before
starting ML-KEM.

### 3. Exact ML-KEM encapsulation and streaming

The client runs FIPS 203 ML-KEM-512 encapsulation against the pinned public
key. The 768 ciphertext bytes are supplied to the transcript hash and packet
writer as they are produced. The v0.1.0 C implementation's largest primitive
writer call is five bytes, while the protocol writer accumulates one 48-byte
network fragment.

Each normal fragment is:

```text
header(10)
fragment_index(1)        # 0..15
fragment_count(1)        # exactly 16
fragment_length(1)       # exactly 48 in suite 0x0001
ciphertext_bytes(48)
tag(16)
```

```text
tag = HMAC16(
    session_auth_key,
    "C0PQ/1 ciphertext fragment",
    header || fragment metadata || ciphertext_bytes
)
```

The peer authenticates a fragment before storing it. Its ACK is:

```text
header(10)
fragment_index(1)
status(1)                # zero means accepted
tag(16)
```

```text
tag = HMAC16(
    session_auth_key,
    "C0PQ/1 ciphertext ack",
    header || fragment_index || status
)
```

The client uses stop-and-wait transmission and retries only the current frame,
up to `maximum_retries` after the first attempt. Duplicate authenticated
fragments and duplicate requests must produce idempotent responses.

### 4. Transcript and key schedule

The transcript excludes the PSK authentication tags but includes the exact
ML-KEM ciphertext:

```text
transcript_hash = SHA-256(
    "C0PQ/1 transcript" ||
    hello_core ||
    challenge_core ||
    mlkem_ciphertext[0..767]
)
```

Session keys combine the PSK and ML-KEM shared secret:

```text
derived_secret   = HKDF-Expand(
    early_secret, "C0PQ/1 derived", 32
)
handshake_secret = HKDF-Extract(
    derived_secret, mlkem_shared_secret
)
```

Each named value is:

```text
HKDF-Expand(
    handshake_secret,
    UTF8(label) || 0x00 || transcript_hash,
    output_length
)
```

| Label | Bytes |
|---|---:|
| `C0PQ/1 device finished` | 32 |
| `C0PQ/1 server finished` | 32 |
| `C0PQ/1 client traffic` | 16 |
| `C0PQ/1 server traffic` | 16 |
| `C0PQ/1 client nonce` | 16 |
| `C0PQ/1 server nonce` | 16 |
| `C0PQ/1 client chain` | 32 |
| `C0PQ/1 server chain` | 32 |

### 5. Bilateral Finished

The client sends Device Finished and accepts application data only after
validating Peer Finished.

```text
finished_tag = HMAC16(
    directional_finished_key,
    "C0PQ/1 finished tag",
    finished_header || transcript_hash
)
```

Both Finished frames are exactly 26 bytes. Failed verification erases
handshake state and leaves the client failed rather than partially
established.

## Application records

The maximum plaintext is 48 bytes:

```text
header(10)
sequence_number(8)
ciphertext(0..48)
ascon_tag(16)
```

The common header and sequence number are Ascon associated data. A
direction-specific base nonce is copied and the big-endian 64-bit sequence
number is XORed into its last eight bytes. Sequence numbers begin at zero,
must arrive exactly in order, and may never wrap.

Authentication failure does not advance the receive sequence or ratchet. A
replayed, skipped, or reordered record is rejected. The transport may provide
its own reordering queue, but it must present records to this layer in order.

A successful `seal` advances the sender state once. If delivery is uncertain,
the caller must retain and retransmit the **exact sealed frame bytes**; calling
`seal` again creates the next sequence and key. A stop-and-wait peer may cache
the most recent authenticated request and its response so an exact duplicate
request receives the exact cached response without repeating application side
effects. The reference peer implements this one-record cache.

One-way telemetry still needs an application or transport acknowledgment if
reliable delivery is required. C0-PQLink authentication does not itself prove
that the remote application consumed a record.

### `FULL_PQ_EACH_SESSION`

The 16-byte directional traffic key is used for the lifetime of one session.
Every record receives a unique nonce from the sequence number. A reconnect
runs a fresh ML-KEM exchange.

### `PQ_BOOTSTRAP_RATCHET`

Before a record:

```text
message_key    = first16(HMAC-SHA-256(
    chain_key, "C0PQ/1 ratchet message"
))
next_chain_key = HMAC-SHA-256(
    chain_key, "C0PQ/1 ratchet next"
)
```

The sender commits `next_chain_key` after encryption. The receiver commits it
only after successful authentication. This is unilateral symmetric key
evolution, not a Signal-style double ratchet.

## Failure behavior

- Parse or state failures return `C0PQLINK_ERR_STATE`.
- Authentication failures return `C0PQLINK_ERR_AUTH`.
- Duplicate/out-of-order records return `C0PQLINK_ERR_REPLAY`.
- Random-source failures return `C0PQLINK_ERR_RNG`.
- Exhausted retries return `C0PQLINK_ERR_IO`.
- Oversized records or output buffers return `C0PQLINK_ERR_CAPACITY`.
- Secret temporaries and the workspace are explicitly zeroed on success and
  failure, subject to compiler/platform behavior that must be validated.

## Versioning rule

A future change to any encoding, label, transcript input, derivation, retry
meaning, or algorithm requires a new wire version or suite ID. An
implementation must not silently reinterpret suite `0x0001`.
