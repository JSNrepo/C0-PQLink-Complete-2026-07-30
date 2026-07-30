# Application adapter contract

`LiveSensorClient.ino` is deliberately transport- and board-neutral. It
compiles with fail-closed adapters: key reads return zero, entropy fails, and
packet I/O fails. Replace the bodies of `DevicePublicKey::read`,
`DeviceRandom::fill`, `DeviceTransport::send`, and
`DeviceTransport::receive` with APIs provided by the target board and modem.
Provision the generated device ID, PSK, public-key ID, public key, and epoch.

The public-key hook must return byte `offset` from the canonical 800-byte
ML-KEM-512 public key. On AVR, implement it with `pgm_read_byte()` so the key
never occupies SRAM. The random hook must return zero only after filling the
requested buffer from a cryptographically secure source. C0-PQLink
intentionally provides no `analogRead()` entropy fallback.

`send()` must consume the frame before returning. `receive()` must place one
complete C0-PQLink frame in the supplied buffer, set its length, and return
zero. On timeout or packet loss it returns nonzero; the library performs
bounded retransmission. Maximum emitted handshake frames are 84 bytes and
records are at most 82 bytes.

After `seal()` succeeds, sending state has advanced. Retain the sealed frame
until delivery is acknowledged and retransmit those exact bytes after loss.
Do not call `seal()` again for the same logical message. One-way telemetry
needs an application or transport acknowledgment when reliable delivery is
required.

Generate paired demo provisioning with:

```sh
npm ci
npm run provision:demo
```

The generated peer configuration contains private material. Do not embed it
in the device or commit it to source control.

For an AVR public-key adapter after copying the generated header into the
sketch directory:

```cpp
#include <avr/pgmspace.h>
#include "c0pq_demo_provisioning.h"

uint8_t DevicePublicKey::read(uint16_t offset) {
  return pgm_read_byte(c0pq_demo_server_public_key + offset);
}
```

Copy the generated device ID, PSK, and key ID through the platform's
provisioning mechanism. The generated source arrays are only for a controlled
demo; production secrets should not live in a sketch.
