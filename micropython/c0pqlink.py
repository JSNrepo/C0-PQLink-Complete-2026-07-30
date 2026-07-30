"""MicroPython facade for the native C0-PQLink live-session module."""

import os

from _c0pqlink import (
    _Client,
    ESTABLISHED,
    FRAME_MAX,
    FULL_PQ_EACH_SESSION,
    PQ_BOOTSTRAP_RATCHET,
    RECORD_MAX,
)


class Client:
    """Transport-neutral post-quantum live-session client.

    ``public_key`` is either the canonical 800-byte ML-KEM-512 public key or
    a callable ``public_key(offset) -> int`` backed by frozen flash.
    ``transport`` supplies ``send(frame)`` and
    ``receive(capacity, timeout_ms) -> bytes | None``.
    ``rng(length)`` must return cryptographically secure bytes; ``os.urandom``
    is used only when the port documents that it is a secure source.
    """

    def __init__(
        self,
        device_id,
        psk,
        epoch,
        public_key_id,
        public_key,
        transport,
        rng=os.urandom,
        mode=PQ_BOOTSTRAP_RATCHET,
        timeout_ms=3000,
        maximum_retries=3,
    ):
        self._native = _Client(
            device_id,
            psk,
            epoch,
            public_key_id,
            public_key,
            rng,
            transport.send,
            transport.receive,
            mode,
            timeout_ms,
            maximum_retries,
        )

    def connect(self):
        self._native.connect()
        return self

    def seal(self, plaintext):
        """Seal once; retransmit the returned bytes unchanged after loss."""
        return self._native.seal(plaintext)

    def open(self, frame):
        return self._native.open(frame)

    def close(self):
        self._native.close()

    def state(self):
        return self._native.state()


__all__ = (
    "Client",
    "ESTABLISHED",
    "FRAME_MAX",
    "FULL_PQ_EACH_SESSION",
    "PQ_BOOTSTRAP_RATCHET",
    "RECORD_MAX",
)
