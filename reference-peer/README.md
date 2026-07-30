# Replaceable reference peer

This directory is not device-side code and is not a cloud cryptography
service. It is the independently implemented opposite endpoint used to prove
that C0-PQLink protects a real live connection.

In ML-KEM, the constrained client encapsulates with the peer's public key and
the key-owning peer decapsulates with its private key. That role split is the
standard KEM operation; no client computation is offloaded. The device still
performs exact ML-KEM-512 encapsulation, creates every ciphertext byte,
authenticates and fragments it, verifies bilateral Finished, and protects
traffic with Ascon.

The Node implementation is replaceable by a gateway, embedded Linux daemon,
telecom endpoint, or backend written in another language. It is also the
independent oracle in the cross-language test, using `@noble/post-quantum` for
ML-KEM-512 decapsulation rather than calling the device C implementation.

To run the UDP reference peer:

```sh
npm ci
npm run provision:demo
cp generated/reference-peer-config.json reference-peer/config.json
npm run peer
```

Copy `generated/c0pq_demo_provisioning.h` into the device application and
adapt its flash read. The JSON contains the private key seed and device PSK;
keep it private. `config.example.json` is a public format example with
deliberately non-secret values and must never be used for a real device.

The UDP wrapper is a demonstration transport, not a hardened production
daemon. It keeps incomplete sessions in memory and has no persistence,
rate-limiting, authorization layer, observability, multi-process state, or
key-management integration. Production endpoints must add those controls and
follow `docs/THREAT-MODEL.md`.
