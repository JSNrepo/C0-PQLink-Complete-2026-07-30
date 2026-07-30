# Threat model

## Protected against in the demonstrated model

- passive capture of live sensor and command records;
- modification of a record or its authenticated header;
- replay within a session;
- reuse of the previous session after a normal reset;
- unauthorized initial policy enrollment without a valid LMS or SLH-DSA
  signature;
- UART overrun during expensive signature verification, through
  verifier-driven flow control;
- accidental/torn EEPROM journal writes.

## Required assumptions

- each production device receives a unique, uniformly random 256-bit root;
- the peer stores the matching root confidentially;
- the post-quantum verification key in flash is authentic;
- the peer obtains an unpredictable server nonce;
- the peer persists the greatest accepted boot epoch;
- LMS authorities allocate each LM-OTS leaf once and only once;
- compiled code and provisioning values are installed through a trusted path;
- SHA-256, HMAC-SHA-256, the selected hash signature, and Ascon-AEAD128 retain
  their expected security.

## Not provided

- public-key key agreement or KEM semantics;
- public-key forward secrecy;
- recovery if the 256-bit device root is exposed;
- protection from invasive physical extraction, voltage/clock glitching, or
  arbitrary flash/EEPROM rewriting;
- side-channel-resistant certification;
- availability against radio/UART jamming;
- multi-packet reordering windows (the receiver intentionally requires the
  exact next sequence);
- production LMS key management in the included demo signer;
- unlimited EEPROM endurance.

## Failure behavior

- malformed or unauthenticated enrollment is rejected and not persisted;
- a failed Ascon tag does not advance the receive chain;
- a repeated/out-of-order sequence is rejected;
- boot-epoch exhaustion enters a fail-closed blink loop;
- a failed handshake enters the same fail-closed state;
- a state record with invalid magic, schema, authorization value, or CRC is
  ignored.

## Deployment choices

Use LMS/W4 when a non-exporting provisioning authority can guarantee atomic
one-time-key state. Use SLH-DSA when signer-state safety cannot be guaranteed.
Use neither profile as a substitute for a KEM if the product requires
zero-touch public-key onboarding or forward secrecy.

