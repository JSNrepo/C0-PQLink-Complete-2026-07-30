# Third-party components

The device C/C++ and MicroPython core has no runtime third-party library
dependency.

The replaceable JavaScript peer and independent oracle use:

- `@noble/post-quantum` 0.6.1 — MIT License
- its transitive Noble dependencies — MIT License

Exact package versions, integrity hashes, and transitive dependencies are in
`package-lock.json`. Installed `node_modules` are intentionally excluded from
the release ZIP; reproduce them with `npm ci`.

Standards and RFCs referenced by the implementation are not bundled:

- NIST FIPS 203;
- NIST SP 800-232;
- RFC 5869;
- RFC 8446;
- RFC 4944.

The C0-PQLink protocol is not endorsed by the authors or publishers of those
documents.
