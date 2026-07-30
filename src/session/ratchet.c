#include "c0pqlink/client.h"

#include "session/internal.h"

#include <string.h>

void c0pq_ratchet_step(
    const uint8_t chain_key[32],
    uint8_t message_key[16],
    uint8_t next_chain_key[32]
)
{
    static const uint8_t message_label[] = "C0PQ/1 ratchet message";
    static const uint8_t next_label[] = "C0PQ/1 ratchet next";
    uint8_t digest[32];
    c0_hmac_sha256(
        digest,
        chain_key,
        32u,
        message_label,
        sizeof(message_label) - 1u
    );
    memcpy(message_key, digest, 16u);
    c0_hmac_sha256(
        next_chain_key,
        chain_key,
        32u,
        next_label,
        sizeof(next_label) - 1u
    );
    c0pqlink_secure_zero(digest, sizeof(digest));
}

void c0pq_make_record_nonce(
    uint8_t nonce[16],
    const uint8_t base[16],
    uint64_t sequence
)
{
    uint8_t encoded[8];
    unsigned int i;
    memcpy(nonce, base, 16u);
    c0pq_store64_be(encoded, sequence);
    for (i = 0u; i < 8u; ++i) {
        nonce[8u + i] ^= encoded[i];
    }
}

static void select_send_key(
    const c0pq_client *client,
    uint8_t message_key[16],
    uint8_t next_chain_key[32]
)
{
    if (client->config.mode == C0PQ_PQ_BOOTSTRAP_RATCHET) {
        c0pq_ratchet_step(
            client->send_secret,
            message_key,
            next_chain_key
        );
    } else {
        memcpy(message_key, client->send_secret, 16u);
        memset(next_chain_key, 0, 32u);
    }
}

static void select_receive_key(
    const c0pq_client *client,
    uint8_t message_key[16],
    uint8_t next_chain_key[32]
)
{
    if (client->config.mode == C0PQ_PQ_BOOTSTRAP_RATCHET) {
        c0pq_ratchet_step(
            client->receive_secret,
            message_key,
            next_chain_key
        );
    } else {
        memcpy(message_key, client->receive_secret, 16u);
        memset(next_chain_key, 0, 32u);
    }
}

int c0pq_client_seal_record(
    c0pq_client *client,
    const uint8_t *plaintext,
    size_t plaintext_length,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *frame_length
)
{
    uint8_t message_key[16];
    uint8_t next_chain_key[32];
    uint8_t nonce[16];
    const size_t needed = C0PQ_FRAME_HEADER_BYTES + 8u
        + plaintext_length + C0_ASCON_AEAD128_TAG_BYTES;
    if (client == NULL || frame == NULL || frame_length == NULL
        || (plaintext == NULL && plaintext_length != 0u)) {
        return C0PQLINK_ERR_ARGUMENT;
    }
    if (client->state != C0PQ_CLIENT_ESTABLISHED) {
        return C0PQLINK_ERR_STATE;
    }
    if (plaintext_length > C0PQ_RECORD_PLAINTEXT_MAX
        || needed > frame_capacity) {
        return C0PQLINK_ERR_CAPACITY;
    }
    if (client->send_sequence == UINT64_MAX) {
        return C0PQLINK_ERR_EXPIRED;
    }
    (void)c0pq_encode_header(
        frame,
        C0PQ_FRAME_DATA,
        0u,
        (uint16_t)(8u + plaintext_length + C0_ASCON_AEAD128_TAG_BYTES),
        client->session_id
    );
    c0pq_store64_be(frame + C0PQ_FRAME_HEADER_BYTES, client->send_sequence);
    select_send_key(client, message_key, next_chain_key);
    c0pq_make_record_nonce(
        nonce,
        client->send_nonce_base,
        client->send_sequence
    );
    c0_ascon_aead128_encrypt(
        frame + C0PQ_FRAME_HEADER_BYTES + 8u,
        frame + C0PQ_FRAME_HEADER_BYTES + 8u + plaintext_length,
        message_key,
        nonce,
        frame,
        C0PQ_FRAME_HEADER_BYTES + 8u,
        plaintext,
        plaintext_length
    );
    if (client->config.mode == C0PQ_PQ_BOOTSTRAP_RATCHET) {
        memcpy(
            client->send_secret,
            next_chain_key,
            sizeof(next_chain_key)
        );
    }
    ++client->send_sequence;
    *frame_length = needed;
    c0pqlink_secure_zero(message_key, sizeof(message_key));
    c0pqlink_secure_zero(next_chain_key, sizeof(next_chain_key));
    c0pqlink_secure_zero(nonce, sizeof(nonce));
    return C0PQLINK_OK;
}

int c0pq_client_open_record(
    c0pq_client *client,
    const uint8_t *frame,
    size_t frame_length,
    uint8_t *plaintext,
    size_t plaintext_capacity,
    size_t *plaintext_length
)
{
    c0pq_frame_header header;
    uint8_t message_key[16];
    uint8_t next_chain_key[32];
    uint8_t nonce[16];
    uint64_t sequence;
    size_t ciphertext_length;
    int result;
    if (client == NULL || frame == NULL || plaintext == NULL
        || plaintext_length == NULL) {
        return C0PQLINK_ERR_ARGUMENT;
    }
    if (client->state != C0PQ_CLIENT_ESTABLISHED) {
        return C0PQLINK_ERR_STATE;
    }
    result = c0pq_decode_header(&header, frame, frame_length);
    if (result != C0PQLINK_OK || header.type != C0PQ_FRAME_DATA
        || header.flags != 1u || header.session_id != client->session_id
        || header.payload_length < 8u + C0_ASCON_AEAD128_TAG_BYTES) {
        return C0PQLINK_ERR_STATE;
    }
    ciphertext_length =
        header.payload_length - 8u - C0_ASCON_AEAD128_TAG_BYTES;
    if (ciphertext_length > C0PQ_RECORD_PLAINTEXT_MAX
        || ciphertext_length > plaintext_capacity) {
        return C0PQLINK_ERR_CAPACITY;
    }
    sequence = c0pq_load64_be(frame + C0PQ_FRAME_HEADER_BYTES);
    if (sequence != client->receive_sequence) {
        return C0PQLINK_ERR_REPLAY;
    }
    select_receive_key(client, message_key, next_chain_key);
    c0pq_make_record_nonce(
        nonce,
        client->receive_nonce_base,
        sequence
    );
    result = c0_ascon_aead128_decrypt(
        plaintext,
        message_key,
        nonce,
        frame,
        C0PQ_FRAME_HEADER_BYTES + 8u,
        frame + C0PQ_FRAME_HEADER_BYTES + 8u,
        ciphertext_length,
        frame + C0PQ_FRAME_HEADER_BYTES + 8u + ciphertext_length
    );
    if (result == C0PQLINK_OK) {
        if (client->config.mode == C0PQ_PQ_BOOTSTRAP_RATCHET) {
            memcpy(
                client->receive_secret,
                next_chain_key,
                sizeof(next_chain_key)
            );
        }
        ++client->receive_sequence;
        *plaintext_length = ciphertext_length;
    } else {
        *plaintext_length = 0u;
    }
    c0pqlink_secure_zero(message_key, sizeof(message_key));
    c0pqlink_secure_zero(next_chain_key, sizeof(next_chain_key));
    c0pqlink_secure_zero(nonce, sizeof(nonce));
    return result;
}
