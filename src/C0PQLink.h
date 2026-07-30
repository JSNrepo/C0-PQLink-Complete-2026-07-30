#ifndef C0PQLINK_ARDUINO_H
#define C0PQLINK_ARDUINO_H

#include "c0pqlink/c0pqlink.h"

namespace C0PQLink {

using Workspace = c0_mlkem512_workspace;

enum SessionMode {
    FULL_PQ_EACH_SESSION = C0PQ_FULL_PQ_EACH_SESSION,
    PQ_BOOTSTRAP_RATCHET = C0PQ_PQ_BOOTSTRAP_RATCHET
};

class PublicKeySource {
public:
    virtual ~PublicKeySource() {}
    virtual uint8_t read(uint16_t offset) = 0;
};

class RandomSource {
public:
    virtual ~RandomSource() {}
    /*
     * Return zero only after filling every byte from a cryptographically
     * secure source. C0-PQLink deliberately has no analogRead() fallback.
     */
    virtual int fill(uint8_t *output, size_t length) = 0;
};

class Transport {
public:
    virtual ~Transport() {}
    virtual int send(const uint8_t *frame, size_t frameLength) = 0;
    virtual int receive(
        uint8_t *frame,
        size_t frameCapacity,
        size_t &frameLength,
        uint32_t timeoutMs
    ) = 0;
};

class Client {
public:
    Client();

    int begin(
        const uint8_t deviceId[C0PQ_DEVICE_ID_BYTES],
        const uint8_t psk[C0PQ_PSK_BYTES],
        uint64_t epoch,
        const uint8_t publicKeyId[C0PQ_KEY_ID_BYTES],
        PublicKeySource &publicKey,
        RandomSource &random,
        Transport &transport,
        SessionMode mode = PQ_BOOTSTRAP_RATCHET,
        uint32_t timeoutMs = 3000u,
        uint8_t maximumRetries = 3u
    );

    int connect(Workspace &workspace);

    /*
     * A successful seal advances send state. Retain the returned frame and
     * retransmit those exact bytes after loss; do not reseal the same record.
     */
    int seal(
        const uint8_t *plaintext,
        size_t plaintextLength,
        uint8_t *frame,
        size_t frameCapacity,
        size_t &frameLength
    );

    int open(
        const uint8_t *frame,
        size_t frameLength,
        uint8_t *plaintext,
        size_t plaintextCapacity,
        size_t &plaintextLength
    );

    void close();
    c0pq_client_state state() const;
    c0pq_client &raw();
    const c0pq_client &raw() const;

private:
    static uint8_t readPublicKey(void *context, uint16_t offset);
    static int fillRandom(void *context, uint8_t *output, size_t length);
    static int sendFrame(
        void *context,
        const uint8_t *frame,
        size_t frameLength
    );
    static int receiveFrame(
        void *context,
        uint8_t *frame,
        size_t frameCapacity,
        size_t *frameLength,
        uint32_t timeoutMs
    );

    c0pq_client client_;
    PublicKeySource *publicKey_;
    RandomSource *random_;
    Transport *transport_;
};

}  // namespace C0PQLink

#endif
