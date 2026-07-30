#include "C0PQLink.h"

#include <string.h>

namespace C0PQLink {

Client::Client()
    : publicKey_(0), random_(0), transport_(0)
{
    memset(&client_, 0, sizeof(client_));
}

int Client::begin(
    const uint8_t deviceId[C0PQ_DEVICE_ID_BYTES],
    const uint8_t psk[C0PQ_PSK_BYTES],
    uint64_t epoch,
    const uint8_t publicKeyId[C0PQ_KEY_ID_BYTES],
    PublicKeySource &publicKey,
    RandomSource &random,
    Transport &transport,
    SessionMode mode,
    uint32_t timeoutMs,
    uint8_t maximumRetries
)
{
    c0pq_client_config config;
    memset(&config, 0, sizeof(config));
    memcpy(config.device_id, deviceId, C0PQ_DEVICE_ID_BYTES);
    memcpy(config.psk, psk, C0PQ_PSK_BYTES);
    config.epoch = epoch;
    memcpy(config.public_key_id, publicKeyId, C0PQ_KEY_ID_BYTES);
    publicKey_ = &publicKey;
    random_ = &random;
    transport_ = &transport;
    config.read_public_key = readPublicKey;
    config.public_key_context = this;
    config.random_bytes = fillRandom;
    config.rng_context = this;
    config.send_frame = sendFrame;
    config.receive_frame = receiveFrame;
    config.transport_context = this;
    config.mode = static_cast<c0pq_session_mode>(mode);
    config.timeout_ms = timeoutMs;
    config.maximum_retries = maximumRetries;
    return c0pq_client_init(&client_, &config);
}

int Client::connect(Workspace &workspace)
{
    return c0pq_client_connect(&client_, &workspace);
}

int Client::seal(
    const uint8_t *plaintext,
    size_t plaintextLength,
    uint8_t *frame,
    size_t frameCapacity,
    size_t &frameLength
)
{
    return c0pq_client_seal_record(
        &client_,
        plaintext,
        plaintextLength,
        frame,
        frameCapacity,
        &frameLength
    );
}

int Client::open(
    const uint8_t *frame,
    size_t frameLength,
    uint8_t *plaintext,
    size_t plaintextCapacity,
    size_t &plaintextLength
)
{
    return c0pq_client_open_record(
        &client_,
        frame,
        frameLength,
        plaintext,
        plaintextCapacity,
        &plaintextLength
    );
}

void Client::close()
{
    c0pq_client_close(&client_);
}

c0pq_client_state Client::state() const
{
    return client_.state;
}

c0pq_client &Client::raw()
{
    return client_;
}

const c0pq_client &Client::raw() const
{
    return client_;
}

uint8_t Client::readPublicKey(void *context, uint16_t offset)
{
    Client *client = static_cast<Client *>(context);
    return client->publicKey_->read(offset);
}

int Client::fillRandom(void *context, uint8_t *output, size_t length)
{
    Client *client = static_cast<Client *>(context);
    return client->random_->fill(output, length);
}

int Client::sendFrame(
    void *context,
    const uint8_t *frame,
    size_t frameLength
)
{
    Client *client = static_cast<Client *>(context);
    return client->transport_->send(frame, frameLength);
}

int Client::receiveFrame(
    void *context,
    uint8_t *frame,
    size_t frameCapacity,
    size_t *frameLength,
    uint32_t timeoutMs
)
{
    Client *client = static_cast<Client *>(context);
    size_t length = 0u;
    const int result = client->transport_->receive(
        frame,
        frameCapacity,
        length,
        timeoutMs
    );
    *frameLength = length;
    return result;
}

}  // namespace C0PQLink
