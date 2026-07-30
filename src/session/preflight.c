#include "c0pqlink/protocol.h"

#include "session/internal.h"

#include <string.h>

int c0pq_encode_header(
    uint8_t output[C0PQ_FRAME_HEADER_BYTES],
    uint8_t type,
    uint8_t flags,
    uint16_t payload_length,
    uint32_t session_id
)
{
    if (output == NULL || payload_length > 255u) {
        return C0PQLINK_ERR_ARGUMENT;
    }
    output[0] = (uint8_t)'C';
    output[1] = (uint8_t)'0';
    output[2] = C0PQ_WIRE_VERSION;
    output[3] = type;
    output[4] = flags;
    output[5] = (uint8_t)payload_length;
    c0pq_store32_be(output + 6u, session_id);
    return C0PQLINK_OK;
}

int c0pq_decode_header(
    c0pq_frame_header *header,
    const uint8_t *frame,
    size_t frame_length
)
{
    if (header == NULL || frame == NULL
        || frame_length < C0PQ_FRAME_HEADER_BYTES) {
        return C0PQLINK_ERR_ARGUMENT;
    }
    if (frame[0] != (uint8_t)'C' || frame[1] != (uint8_t)'0'
        || frame[2] != C0PQ_WIRE_VERSION) {
        return C0PQLINK_ERR_STATE;
    }
    header->type = frame[3];
    header->flags = frame[4];
    header->payload_length = frame[5];
    header->session_id = c0pq_load32_be(frame + 6u);
    if ((size_t)header->payload_length + C0PQ_FRAME_HEADER_BYTES
        != frame_length) {
        return C0PQLINK_ERR_STATE;
    }
    return C0PQLINK_OK;
}

int c0pq_encode_hello(
    uint8_t output[C0PQ_HELLO_FRAME_BYTES],
    uint32_t session_id,
    const c0pq_hello *hello,
    const uint8_t psk[C0PQ_PSK_BYTES]
)
{
    uint8_t hello_key[32];
    if (output == NULL || hello == NULL || psk == NULL
        || hello->suite != C0PQ_SUITE_MLKEM512_HKDFSHA256_ASCON128) {
        return C0PQLINK_ERR_ARGUMENT;
    }
    (void)c0pq_encode_header(
        output,
        C0PQ_FRAME_HELLO,
        0u,
        C0PQ_HELLO_FRAME_BYTES - C0PQ_FRAME_HEADER_BYTES,
        session_id
    );
    c0pq_store16_be(output + 10u, hello->suite);
    memcpy(output + 12u, hello->device_id, C0PQ_DEVICE_ID_BYTES);
    c0pq_store64_be(output + 20u, hello->epoch);
    memcpy(output + 28u, hello->device_nonce, C0PQ_NONCE_BYTES);
    c0pq_derive_hello_key(hello_key, psk);
    c0pq_auth_tag(
        output + C0PQ_HELLO_CORE_BYTES,
        hello_key,
        "C0PQ/1 hello frame",
        output,
        C0PQ_HELLO_CORE_BYTES
    );
    c0pqlink_secure_zero(hello_key, sizeof(hello_key));
    return C0PQLINK_OK;
}

int c0pq_decode_verify_hello(
    c0pq_hello *hello,
    uint32_t *session_id,
    const uint8_t *frame,
    size_t frame_length,
    const uint8_t psk[C0PQ_PSK_BYTES]
)
{
    c0pq_frame_header header;
    uint8_t hello_key[32];
    int result;
    if (hello == NULL || session_id == NULL || frame == NULL || psk == NULL
        || frame_length != C0PQ_HELLO_FRAME_BYTES) {
        return C0PQLINK_ERR_ARGUMENT;
    }
    result = c0pq_decode_header(&header, frame, frame_length);
    if (result != C0PQLINK_OK || header.type != C0PQ_FRAME_HELLO
        || header.flags != 0u
        || header.payload_length
            != C0PQ_HELLO_FRAME_BYTES - C0PQ_FRAME_HEADER_BYTES) {
        return C0PQLINK_ERR_STATE;
    }
    c0pq_derive_hello_key(hello_key, psk);
    result = c0pq_verify_auth_tag(
        frame + C0PQ_HELLO_CORE_BYTES,
        hello_key,
        "C0PQ/1 hello frame",
        frame,
        C0PQ_HELLO_CORE_BYTES
    );
    c0pqlink_secure_zero(hello_key, sizeof(hello_key));
    if (result != C0PQLINK_OK) {
        return result;
    }
    hello->suite = c0pq_load16_be(frame + 10u);
    if (hello->suite != C0PQ_SUITE_MLKEM512_HKDFSHA256_ASCON128) {
        c0pqlink_secure_zero(hello, sizeof(*hello));
        return C0PQLINK_ERR_STATE;
    }
    memcpy(hello->device_id, frame + 12u, C0PQ_DEVICE_ID_BYTES);
    hello->epoch = c0pq_load64_be(frame + 20u);
    memcpy(hello->device_nonce, frame + 28u, C0PQ_NONCE_BYTES);
    *session_id = header.session_id;
    return C0PQLINK_OK;
}

int c0pq_encode_challenge(
    uint8_t output[C0PQ_CHALLENGE_FRAME_BYTES],
    uint32_t session_id,
    const c0pq_challenge *challenge,
    const uint8_t psk[C0PQ_PSK_BYTES]
)
{
    uint8_t session_auth_key[32];
    if (output == NULL || challenge == NULL || psk == NULL
        || challenge->suite
            != C0PQ_SUITE_MLKEM512_HKDFSHA256_ASCON128) {
        return C0PQLINK_ERR_ARGUMENT;
    }
    (void)c0pq_encode_header(
        output,
        C0PQ_FRAME_CHALLENGE,
        0u,
        C0PQ_CHALLENGE_FRAME_BYTES - C0PQ_FRAME_HEADER_BYTES,
        session_id
    );
    c0pq_store16_be(output + 10u, challenge->suite);
    c0pq_store64_be(output + 12u, challenge->epoch);
    memcpy(output + 20u, challenge->key_id, C0PQ_KEY_ID_BYTES);
    memcpy(output + 36u, challenge->device_nonce, C0PQ_NONCE_BYTES);
    memcpy(output + 52u, challenge->server_nonce, C0PQ_NONCE_BYTES);
    c0pq_derive_session_auth_key(
        session_auth_key,
        psk,
        challenge->suite,
        challenge->epoch,
        challenge->key_id,
        challenge->device_nonce,
        challenge->server_nonce
    );
    c0pq_auth_tag(
        output + C0PQ_CHALLENGE_CORE_BYTES,
        session_auth_key,
        "C0PQ/1 challenge frame",
        output,
        C0PQ_CHALLENGE_CORE_BYTES
    );
    c0pqlink_secure_zero(session_auth_key, sizeof(session_auth_key));
    return C0PQLINK_OK;
}

int c0pq_decode_verify_challenge(
    c0pq_challenge *challenge,
    uint32_t expected_session_id,
    const uint8_t *frame,
    size_t frame_length,
    const uint8_t psk[C0PQ_PSK_BYTES]
)
{
    c0pq_frame_header header;
    uint8_t session_auth_key[32];
    int result;
    if (challenge == NULL || frame == NULL || psk == NULL
        || frame_length != C0PQ_CHALLENGE_FRAME_BYTES) {
        return C0PQLINK_ERR_ARGUMENT;
    }
    result = c0pq_decode_header(&header, frame, frame_length);
    if (result != C0PQLINK_OK || header.type != C0PQ_FRAME_CHALLENGE
        || header.flags != 0u
        || header.session_id != expected_session_id
        || header.payload_length
            != C0PQ_CHALLENGE_FRAME_BYTES - C0PQ_FRAME_HEADER_BYTES) {
        return C0PQLINK_ERR_STATE;
    }
    challenge->suite = c0pq_load16_be(frame + 10u);
    challenge->epoch = c0pq_load64_be(frame + 12u);
    memcpy(challenge->key_id, frame + 20u, C0PQ_KEY_ID_BYTES);
    memcpy(challenge->device_nonce, frame + 36u, C0PQ_NONCE_BYTES);
    memcpy(challenge->server_nonce, frame + 52u, C0PQ_NONCE_BYTES);
    if (challenge->suite
        != C0PQ_SUITE_MLKEM512_HKDFSHA256_ASCON128) {
        c0pqlink_secure_zero(challenge, sizeof(*challenge));
        return C0PQLINK_ERR_STATE;
    }
    c0pq_derive_session_auth_key(
        session_auth_key,
        psk,
        challenge->suite,
        challenge->epoch,
        challenge->key_id,
        challenge->device_nonce,
        challenge->server_nonce
    );
    result = c0pq_verify_auth_tag(
        frame + C0PQ_CHALLENGE_CORE_BYTES,
        session_auth_key,
        "C0PQ/1 challenge frame",
        frame,
        C0PQ_CHALLENGE_CORE_BYTES
    );
    c0pqlink_secure_zero(session_auth_key, sizeof(session_auth_key));
    if (result != C0PQLINK_OK) {
        c0pqlink_secure_zero(challenge, sizeof(*challenge));
    }
    return result;
}
