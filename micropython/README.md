# MicroPython native module

The Python API is a thin facade over the same C core used by Arduino. It is
not a pure-Python reimplementation.

For CMake-based ports, add:

```sh
make USER_C_MODULES=/absolute/path/C0-PQLink/micropython/micropython.cmake \
     FROZEN_MANIFEST=/absolute/path/C0-PQLink/micropython/manifest.py
```

For Make-based ports, point `USER_C_MODULES` at the C0-PQLink project root
(the parent of `micropython`), because Make searches it for module
subdirectories. Exact command paths vary by port:

```sh
make USER_C_MODULES=/absolute/path/C0-PQLink \
     FROZEN_MANIFEST=/absolute/path/C0-PQLink/micropython/manifest.py
```

```python
from c0pqlink import Client

client = Client(
    device_id,
    psk,
    epoch,
    server_key_id,
    frozen_public_key_reader,  # or an 800-byte frozen buffer
    packet_transport,
    rng=board_secure_random,
).connect()

packet_transport.send(client.seal(b"temperature=24.5"))
reply = client.open(packet_transport.receive(96, 3000))
```

Store the result of `seal()` until delivery is acknowledged. Retransmit the
same `bytes` object after packet loss; do not call `seal()` again for the same
logical record because send sequence and ratchet state advance on a
successful seal.

The native object owns the 1,344-byte ML-KEM workspace. `close()` erases live
session secrets and the workspace. A callback-backed public key avoids an
800-byte heap copy. If an 800-byte `bytes` object is used, freeze it into
firmware so the port can keep it in flash.

The facade defaults to `os.urandom`, but only use that default on ports whose
documentation guarantees a cryptographically secure implementation. Pass an
explicit secure-element/TRNG callback otherwise. There is no weak entropy
fallback.
