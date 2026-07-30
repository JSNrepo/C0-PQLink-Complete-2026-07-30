#include "c0pqlink/client.h"

#include "session/internal.h"

#include <string.h>

typedef struct {
    c0pq_client *client;
    c0_sha256_ctx *transcript;
    uint8_t session_auth_key[32];
    uint8_t pending[C0PQ_FRAGMENT_PAYLOAD_BYTES];
    uint8_t pending_length;
    uint8_t fragment_index;
    int failure;
} ciphertext_writer;

static int send_and_receive(
    c0pq_client *client,
    const uint8_t *outgoing,
    size_t outgoing_length,
    uint8_t expected_type,
    size_t expected_length,
    uint8_t *incoming
)
{
    unsigned int attempt;
    for (attempt = 0u;
         attempt <= (unsigned int)client->config.maximum_retries;
         ++attempt) {
        c0pq_frame_header header;
        size_t incoming_length = 0u;
        if (client->config.send_frame(
                client->config.transport_context,
                outgoing,
                outgoing_length
            ) != 0) {
            continue;
        }
        if (client->config.receive_frame(
                client->config.transport_context,
                incoming,
                C0PQ_FRAME_MAX_BYTES,
                &incoming_length,
                client->config.timeout_ms
            ) != 0) {
            continue;
        }
        if (incoming_length == expected_length
            && c0pq_decode_header(
                &header,
                incoming,
                incoming_length
            ) == C0PQLINK_OK
            && header.type == expected_type
            && header.session_id == client->session_id) {
            return C0PQLINK_OK;
        }
    }
    return C0PQLINK_ERR_IO;
}

static int flush_ciphertext_fragment(ciphertext_writer *writer)
{
    uint8_t frame[C0PQ_FRAME_MAX_BYTES];
    c0pq_frame_header header;
    const size_t core_length =
        C0PQ_FRAME_HEADER_BYTES + 3u + writer->pending_length;
    const size_t frame_length = core_length + C0PQ_AUTH_TAG_BYTES;
    int result;
    if (writer->pending_length == 0u
        || writer->fragment_index >= C0PQ_FRAGMENT_COUNT) {
        return C0PQLINK_ERR_STATE;
    }
    (void)c0pq_encode_header(
        frame,
        C0PQ_FRAME_CIPHERTEXT_FRAGMENT,
        0u,
        (uint16_t)(3u + writer->pending_length + C0PQ_AUTH_TAG_BYTES),
        writer->client->session_id
    );
    frame[10] = writer->fragment_index;
    frame[11] = C0PQ_FRAGMENT_COUNT;
    frame[12] = writer->pending_length;
    memcpy(frame + 13u, writer->pending, writer->pending_length);
    c0pq_auth_tag(
        frame + core_length,
        writer->session_auth_key,
        "C0PQ/1 ciphertext fragment",
        frame,
        core_length
    );
    result = send_and_receive(
        writer->client,
        frame,
        frame_length,
        C0PQ_FRAME_CIPHERTEXT_ACK,
        C0PQ_ACK_FRAME_BYTES,
        frame
    );
    if (result != C0PQLINK_OK) {
        return result;
    }
    result = c0pq_decode_header(&header, frame, C0PQ_ACK_FRAME_BYTES);
    if (result != C0PQLINK_OK || header.flags != 0u
        || header.payload_length
            != C0PQ_ACK_FRAME_BYTES - C0PQ_FRAME_HEADER_BYTES
        || frame[10] != writer->fragment_index
        || frame[11] != 0u) {
        return C0PQLINK_ERR_STATE;
    }
    result = c0pq_verify_auth_tag(
        frame + 12u,
        writer->session_auth_key,
        "C0PQ/1 ciphertext ack",
        frame,
        12u
    );
    if (result != C0PQLINK_OK) {
        return result;
    }
    ++writer->fragment_index;
    writer->pending_length = 0u;
    return C0PQLINK_OK;
}

static int write_ciphertext(
    void *context,
    const uint8_t *data,
    size_t length
)
{
    ciphertext_writer *writer = (ciphertext_writer *)context;
    size_t offset = 0u;
    if (writer->failure != C0PQLINK_OK) {
        return writer->failure;
    }
    c0_sha256_update(writer->transcript, data, length);
    while (offset < length) {
        const size_t space =
            C0PQ_FRAGMENT_PAYLOAD_BYTES - writer->pending_length;
        const size_t take = length - offset < space
            ? length - offset : space;
        memcpy(writer->pending + writer->pending_length, data + offset, take);
        writer->pending_length = (uint8_t)(writer->pending_length + take);
        offset += take;
        if (writer->pending_length == C0PQ_FRAGMENT_PAYLOAD_BYTES) {
            writer->failure = flush_ciphertext_fragment(writer);
            if (writer->failure != C0PQLINK_OK) {
                return writer->failure;
            }
        }
    }
    return C0PQLINK_OK;
}

int c0pq_client_init(
    c0pq_client *client,
    const c0pq_client_config *config
)
{
    uint8_t psk_or = 0u;
    uint8_t key_id_or = 0u;
    unsigned int i;
    if (config != NULL) {
        for (i = 0u; i < C0PQ_PSK_BYTES; ++i) {
            psk_or |= config->psk[i];
        }
        for (i = 0u; i < C0PQ_KEY_ID_BYTES; ++i) {
            key_id_or |= config->public_key_id[i];
        }
    }
    if (client == NULL || config == NULL
        || config->read_public_key == NULL
        || config->random_bytes == NULL
        || config->send_frame == NULL
        || config->receive_frame == NULL
        || psk_or == 0u
        || key_id_or == 0u
        || (config->mode != C0PQ_FULL_PQ_EACH_SESSION
            && config->mode != C0PQ_PQ_BOOTSTRAP_RATCHET)) {
        return C0PQLINK_ERR_ARGUMENT;
    }
    memset(client, 0, sizeof(*client));
    memcpy(&client->config, config, sizeof(*config));
    if (client->config.timeout_ms == 0u) {
        client->config.timeout_ms = 3000u;
    }
    client->state = C0PQ_CLIENT_READY;
    return C0PQLINK_OK;
}

static int verify_challenge_matches(
    const c0pq_client *client,
    const c0pq_challenge *challenge,
    const uint8_t device_nonce[C0PQ_NONCE_BYTES]
)
{
    if (challenge->suite
            != C0PQ_SUITE_MLKEM512_HKDFSHA256_ASCON128
        || challenge->epoch != client->config.epoch
        || !c0pqlink_ct_equal(
            challenge->key_id,
            client->config.public_key_id,
            C0PQ_KEY_ID_BYTES
        )
        || !c0pqlink_ct_equal(
            challenge->device_nonce,
            device_nonce,
            C0PQ_NONCE_BYTES
        )) {
        return C0PQLINK_ERR_AUTH;
    }
    return C0PQLINK_OK;
}

static int exchange_finished(
    c0pq_client *client,
    const c0pq_session_keys *keys,
    const uint8_t transcript_hash[32]
)
{
    uint8_t frame[C0PQ_FRAME_MAX_BYTES];
    uint8_t expected[C0PQ_FINISHED_BYTES];
    c0pq_frame_header header;
    int result;
    (void)c0pq_encode_header(
        frame,
        C0PQ_FRAME_DEVICE_FINISHED,
        0u,
        C0PQ_FINISHED_BYTES,
        client->session_id
    );
    c0pq_finished_tag(
        frame + C0PQ_FRAME_HEADER_BYTES,
        keys->device_finished_key,
        C0PQ_FRAME_DEVICE_FINISHED,
        client->session_id,
        transcript_hash
    );
    result = send_and_receive(
        client,
        frame,
        C0PQ_FINISHED_FRAME_BYTES,
        C0PQ_FRAME_SERVER_FINISHED,
        C0PQ_FINISHED_FRAME_BYTES,
        frame
    );
    if (result != C0PQLINK_OK) {
        return result;
    }
    result = c0pq_decode_header(
        &header,
        frame,
        C0PQ_FINISHED_FRAME_BYTES
    );
    if (result != C0PQLINK_OK || header.flags != 0u
        || header.payload_length != C0PQ_FINISHED_BYTES) {
        return C0PQLINK_ERR_STATE;
    }
    c0pq_finished_tag(
        expected,
        keys->server_finished_key,
        C0PQ_FRAME_SERVER_FINISHED,
        client->session_id,
        transcript_hash
    );
    result = c0pqlink_ct_equal(
        expected,
        frame + C0PQ_FRAME_HEADER_BYTES,
        sizeof(expected)
    ) ? C0PQLINK_OK : C0PQLINK_ERR_AUTH;
    c0pqlink_secure_zero(expected, sizeof(expected));
    return result;
}

int c0pq_client_connect(
    c0pq_client *client,
    c0_mlkem512_workspace *workspace
)
{
    uint8_t handshake_frame[C0PQ_FRAME_MAX_BYTES];
    uint8_t device_nonce[C0PQ_NONCE_BYTES];
    uint8_t shared_secret[32];
    uint8_t transcript_hash[32];
    c0pq_hello hello;
    c0pq_challenge challenge;
    c0_sha256_ctx transcript;
    ciphertext_writer writer;
    c0pq_session_keys *session_keys;
    int result;
    if (client == NULL || workspace == NULL
        || client->state != C0PQ_CLIENT_READY) {
        return C0PQLINK_ERR_STATE;
    }
    memset(device_nonce, 0, sizeof(device_nonce));
    memset(shared_secret, 0, sizeof(shared_secret));
    memset(transcript_hash, 0, sizeof(transcript_hash));
    memset(&transcript, 0, sizeof(transcript));
    memset(&writer, 0, sizeof(writer));
    memset(&hello, 0, sizeof(hello));
    memset(&challenge, 0, sizeof(challenge));
    client->state = C0PQ_CLIENT_CONNECTING;
    if (client->config.random_bytes(
            client->config.rng_context,
            device_nonce,
            C0PQ_NONCE_BYTES
        ) != 0) {
        result = C0PQLINK_ERR_RNG;
        goto fail;
    }
    client->session_id =
        c0pq_load32_be(device_nonce) ^ UINT32_C(0xc0505101);
    hello.suite = C0PQ_SUITE_MLKEM512_HKDFSHA256_ASCON128;
    memcpy(hello.device_id, client->config.device_id, C0PQ_DEVICE_ID_BYTES);
    hello.epoch = client->config.epoch;
    memcpy(hello.device_nonce, device_nonce, C0PQ_NONCE_BYTES);
    result = c0pq_encode_hello(
        handshake_frame,
        client->session_id,
        &hello,
        client->config.psk
    );
    if (result != C0PQLINK_OK) {
        goto fail;
    }
    c0pq_transcript_init(&transcript);
    c0pq_transcript_add_hello(&transcript, handshake_frame);
    result = send_and_receive(
        client,
        handshake_frame,
        C0PQ_HELLO_FRAME_BYTES,
        C0PQ_FRAME_CHALLENGE,
        C0PQ_CHALLENGE_FRAME_BYTES,
        handshake_frame
    );
    if (result != C0PQLINK_OK) {
        goto fail;
    }
    result = c0pq_decode_verify_challenge(
        &challenge,
        client->session_id,
        handshake_frame,
        C0PQ_CHALLENGE_FRAME_BYTES,
        client->config.psk
    );
    if (result != C0PQLINK_OK) {
        goto fail;
    }
    result = verify_challenge_matches(client, &challenge, device_nonce);
    if (result != C0PQLINK_OK) {
        goto fail;
    }
    c0pq_transcript_add_challenge(&transcript, handshake_frame);
    writer.client = client;
    writer.transcript = &transcript;
    c0pq_derive_session_auth_key(
        writer.session_auth_key,
        client->config.psk,
        challenge.suite,
        challenge.epoch,
        challenge.key_id,
        challenge.device_nonce,
        challenge.server_nonce
    );
    result = c0_mlkem512_encapsulate(
        client->config.read_public_key,
        client->config.public_key_context,
        client->config.random_bytes,
        client->config.rng_context,
        write_ciphertext,
        &writer,
        shared_secret,
        workspace
    );
    if (result == C0PQLINK_OK && writer.pending_length != 0u) {
        result = flush_ciphertext_fragment(&writer);
    }
    if (result == C0PQLINK_OK
        && (writer.fragment_index != C0PQ_FRAGMENT_COUNT
            || writer.failure != C0PQLINK_OK)) {
        result = C0PQLINK_ERR_IO;
    }
    c0pqlink_secure_zero(
        writer.session_auth_key,
        sizeof(writer.session_auth_key)
    );
    if (result != C0PQLINK_OK) {
        c0pqlink_secure_zero(shared_secret, sizeof(shared_secret));
        goto fail;
    }
    c0_sha256_final(&transcript, transcript_hash);
    session_keys = (c0pq_session_keys *)(void *)workspace->bytes;
    result = c0pq_derive_session_keys(
        session_keys,
        client->config.psk,
        shared_secret,
        transcript_hash
    );
    c0pqlink_secure_zero(shared_secret, sizeof(shared_secret));
    if (result != C0PQLINK_OK) {
        goto fail;
    }
    result = exchange_finished(client, session_keys, transcript_hash);
    if (result != C0PQLINK_OK) {
        goto fail;
    }
    memset(client->send_secret, 0, sizeof(client->send_secret));
    memset(client->receive_secret, 0, sizeof(client->receive_secret));
    if (client->config.mode == C0PQ_PQ_BOOTSTRAP_RATCHET) {
        memcpy(
            client->send_secret,
            session_keys->client_chain_key,
            sizeof(client->send_secret)
        );
        memcpy(
            client->receive_secret,
            session_keys->server_chain_key,
            sizeof(client->receive_secret)
        );
    } else {
        memcpy(
            client->send_secret,
            session_keys->client_traffic_key,
            16u
        );
        memcpy(
            client->receive_secret,
            session_keys->server_traffic_key,
            16u
        );
    }
    memcpy(
        client->send_nonce_base,
        session_keys->client_nonce_base,
        sizeof(client->send_nonce_base)
    );
    memcpy(
        client->receive_nonce_base,
        session_keys->server_nonce_base,
        sizeof(client->receive_nonce_base)
    );
    c0pqlink_secure_zero(workspace, sizeof(*workspace));
    c0pqlink_secure_zero(transcript_hash, sizeof(transcript_hash));
    c0pqlink_secure_zero(device_nonce, sizeof(device_nonce));
    client->send_sequence = 0u;
    client->receive_sequence = 0u;
    client->state = C0PQ_CLIENT_ESTABLISHED;
    c0pqlink_secure_zero(&hello, sizeof(hello));
    c0pqlink_secure_zero(&challenge, sizeof(challenge));
    return C0PQLINK_OK;

fail:
    c0pqlink_secure_zero(workspace, sizeof(*workspace));
    c0pqlink_secure_zero(&transcript, sizeof(transcript));
    c0pqlink_secure_zero(shared_secret, sizeof(shared_secret));
    c0pqlink_secure_zero(transcript_hash, sizeof(transcript_hash));
    c0pqlink_secure_zero(device_nonce, sizeof(device_nonce));
    c0pqlink_secure_zero(client->send_secret, sizeof(client->send_secret));
    c0pqlink_secure_zero(
        client->receive_secret,
        sizeof(client->receive_secret)
    );
    c0pqlink_secure_zero(
        client->send_nonce_base,
        sizeof(client->send_nonce_base)
    );
    c0pqlink_secure_zero(
        client->receive_nonce_base,
        sizeof(client->receive_nonce_base)
    );
    client->state = C0PQ_CLIENT_FAILED;
    c0pqlink_secure_zero(&hello, sizeof(hello));
    c0pqlink_secure_zero(&challenge, sizeof(challenge));
    return result;
}

void c0pq_client_close(c0pq_client *client)
{
    if (client == NULL) {
        return;
    }
    c0pqlink_secure_zero(client->send_secret, sizeof(client->send_secret));
    c0pqlink_secure_zero(
        client->receive_secret,
        sizeof(client->receive_secret)
    );
    c0pqlink_secure_zero(
        client->send_nonce_base,
        sizeof(client->send_nonce_base)
    );
    c0pqlink_secure_zero(
        client->receive_nonce_base,
        sizeof(client->receive_nonce_base)
    );
    client->send_sequence = 0u;
    client->receive_sequence = 0u;
    client->session_id = 0u;
    client->state = C0PQ_CLIENT_READY;
}
