# Algorithm and protocol

## Design decision

The ATmega328P cannot execute the previous whole-session ML-KEM implementation:
2,016 bytes of static SRAM plus a 611-byte `connect()` frame already requires at
least 2,627 bytes. Optimizing only the traffic cipher cannot repair that
overrun.

NanoPQ-SecureLink changes the frequency and role of public-key cryptography:

1. A post-quantum signature authorizes a 48-byte policy manifest during initial
   enrollment or policy cutover.
2. The Nano streams the signature into a constant-memory verifier.
3. The authorization result and a monotonic policy sequence are journaled in
   EEPROM.
4. Every connection uses a unique boot epoch, a peer nonce, and an independently
   provisioned 256-bit device root to derive fresh traffic state.
5. Ascon-AEAD128 protects short sensor and actuator records.

This is a constrained-device protocol composition, not a newly invented
hardness assumption.

## Authorization profiles

### Default: LMS H5/W4

- LMS type: `LMS_SHA256_M32_H5`
- LM-OTS type: `LMOTS_SHA256_N32_W4`
- public key: 56 bytes
- signature: 2,348 bytes
- tree capacity: 32 one-time leaves for the selected H5 demonstration key
- Nano behavior: verifier only; no private signing state

The verifier consumes the signature as:

\[
q \parallel \text{type} \parallel C \parallel
y_0 \parallel \dots \parallel y_{66} \parallel
\text{LMS type} \parallel \text{path}_0 \parallel \dots \parallel
\text{path}_4.
\]

Only one 32-byte chain value and one 32-byte authentication-path node are live
at a time. The public root is compared in constant time.

W4 wins the measured default decision even though W8 has the smaller signature.
W8 performs up to 255 hash-chain steps per coefficient, while W4 performs up to
15. On the complete compiled AVR path, W4 takes 6.596 seconds and W8 takes
47.092 seconds.

### Alternative: LMS H5/W8

- public key: 56 bytes
- signature: 1,292 bytes
- same Nano SRAM and flash as W4
- useful when wire traffic matters more than enrollment latency

### Stateless alternative: SLH-DSA-SHA2-128s

- FIPS 205 parameter set
- public key: 32 bytes
- signature: 7,856 bytes
- no signer state
- measured authorization: 23.638 seconds at 16 MHz

SLH-DSA is the operationally safer choice when the authority cannot guarantee
atomic LMS leaf allocation.

## Signed policy

The fixed 48-byte manifest is:

| Offset | Bytes | Meaning |
|---:|---:|---|
| 0 | 4 | Magic `QPC1` |
| 4 | 1 | Manifest version |
| 5 | 1 | KDF suite |
| 6 | 1 | Traffic suite |
| 7 | 1 | Authorization mode |
| 8 | 16 | Device-class identifier |
| 24 | 16 | Legacy-policy digest |
| 40 | 4 | Monotonic policy sequence |
| 44 | 4 | Reserved |

The Nano checks the structure, device class, policy digest, and increasing
sequence before accepting the signature. Signature bytes are requested from
the peer only when the verifier is ready, preventing the two-byte AVR UART
hardware buffer from overflowing during hashing.

## Session establishment

Let:

- \(K_R\) be the 32-byte per-device root;
- \(D\) be the 8-byte device identifier;
- \(E\) be the persistent non-zero boot epoch;
- \(N_S\) be the peer's 16-byte random nonce.

The Nano computes a deterministic unique client nonce:

\[
N_C = \operatorname{Trunc}_{16}
  \operatorname{HMAC}_{K_R}(\text{"NPQ/1 client nonce"} \parallel 0
  \parallel D \parallel E).
\]

It sends an authenticated `HELLO` containing \(D,E,N_C\). The peer rejects any
epoch not greater than its persisted last accepted epoch and returns an
authenticated challenge containing \(E,N_C,N_S\).

The transcript is:

\[
T = \operatorname{SHA256}(
  \text{"NPQ/1 transcript"} \parallel \text{HELLO} \parallel
  \text{CHALLENGE}).
\]

Extraction uses the transcript as the HMAC salt:

\[
\mathrm{PRK} = \operatorname{HMAC}_{T}(K_R).
\]

Domain-separated HMAC expansion derives:

- client send chain;
- server send chain;
- client nonce base;
- server nonce base;
- independent client and server Finished keys.

Both sides exchange 16-byte Finished tags before application traffic. The
device root is never sent.

## Record protection

Each direction has an independent 32-byte chain and 32-bit sequence number.
For sequence \(i\):

\[
K_i = \operatorname{Trunc}_{16}
  \operatorname{HMAC}_{C_i}(\text{"NPQ/1 message key"} \parallel 0 \parallel i)
\]

\[
C_{i+1} =
  \operatorname{HMAC}_{C_i}(\text{"NPQ/1 next chain"} \parallel 0 \parallel i).
\]

The 16-byte Ascon nonce is the direction-specific nonce base with \(i\) XORed
into its final four bytes. The 13-byte record header is associated data.
The receive chain advances only after successful tag verification.

Frames are at most 64 bytes:

| Frame | Bytes |
|---|---:|
| Hello | 48 |
| Challenge | 60 |
| Finished | 24 |
| Data overhead | 29 |
| Maximum plaintext | 24 |

## Reset safety

Two 18-byte EEPROM slots form a journal containing:

- schema and authorization flag;
- generation counter;
- boot epoch;
- policy sequence;
- CRC-16.

The inactive slot is written with its magic cleared; all remaining fields are
updated; the magic is written last. On boot, the valid record with the newest
generation is selected. The epoch is incremented and persisted before the
session begins. Epoch exhaustion is fail-closed.

The CRC detects accidental/torn writes; it is not a cryptographic MAC. An
attacker with physical EEPROM write access is outside the basic remote threat
model.

## Security boundary

The combination provides post-quantum signature authorization plus
quantum-resistant symmetric key establishment from a high-entropy 256-bit
pre-shared root. It is not a KEM and does not provide public-key forward
secrecy. Ascon-AEAD128 sets the traffic primitive's standardized security
strength. This implementation has not undergone independent cryptanalysis or
certification.

