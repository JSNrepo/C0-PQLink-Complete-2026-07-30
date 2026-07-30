#include "C0PQLink.h"

#include <string.h>

class TestPublicKey : public C0PQLink::PublicKeySource {
public:
    uint8_t read(uint16_t offset) override
    {
        return static_cast<uint8_t>(offset);
    }
};

class TestRandom : public C0PQLink::RandomSource {
public:
    int fill(uint8_t *output, size_t length) override
    {
        memset(output, 0x42, length);
        return 0;
    }
};

class TestTransport : public C0PQLink::Transport {
public:
    int send(const uint8_t *frame, size_t frameLength) override
    {
        (void)frame;
        (void)frameLength;
        return 0;
    }

    int receive(
        uint8_t *frame,
        size_t frameCapacity,
        size_t &frameLength,
        uint32_t timeoutMs
    ) override
    {
        (void)frame;
        (void)frameCapacity;
        (void)timeoutMs;
        frameLength = 0u;
        return -1;
    }
};

int main()
{
    uint8_t deviceId[C0PQ_DEVICE_ID_BYTES] = { 0 };
    uint8_t psk[C0PQ_PSK_BYTES] = { 0 };
    uint8_t keyId[C0PQ_KEY_ID_BYTES] = { 0 };
    memset(psk, 0x11, sizeof(psk));
    memset(keyId, 0x22, sizeof(keyId));
    TestPublicKey publicKey;
    TestRandom random;
    TestTransport transport;
    C0PQLink::Client client;
    return client.begin(
        deviceId,
        psk,
        1u,
        keyId,
        publicKey,
        random,
        transport
    ) == C0PQLINK_OK ? 0 : 1;
}
