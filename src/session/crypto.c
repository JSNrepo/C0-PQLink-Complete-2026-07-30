#include "c0pqlink/protocol.h"

#include "session/internal.h"

#include <string.h>

static const uint8_t transcript_prefix[] = {
    'C', '0', 'P', 'Q', '/', '1', ' ', 't', 'r', 'a', 'n', 's',
    'c', 'r', 'i', 'p', 't'
};

void c0pq_store16_be(uint8_t output[2], uint16_t value)
{
    output[0] = (uint8_t)(value >> 8u);
    output[1] = (uint8_t)value;
}

void c0pq_store32_be(uint8_t output[4], uint32_t value)
{
    output[0] = (uint8_t)(value >> 24u);
    output[1] = (uint8_t)(value >> 16u);
    output[2] = (uint8_t)(value >> 8u);
    output[3] = (uint8_t)value;
}

void c0pq_store64_be(uint8_t output[8], uint64_t value)
{
    unsigned int i;
    for (i = 0u; i < 8u; ++i) {
        output[7u - i] = (uint8_t)value;
        value >>= 8u;
    }
}

uint16_t c0pq_load16_be(const uint8_t input[2])
{
    return (uint16_t)((uint16_t)input[0] << 8u) | input[1];
}

uint32_t c0pq_load32_be(const uint8_t input[4])
{
    return ((uint32_t)input[0] << 24u)
        | ((uint32_t)input[1] << 16u)
        | ((uint32_t)input[2] << 8u)
        | input[3];
}

uint64_t c0pq_load64_be(const uint8_t input[8])
{
    uint64_t value = 0u;
    unsigned int i;
    for (i = 0u; i < 8u; ++i) {
        value = (value << 8u) | input[i];
    }
    return value;
}

void c0pq_auth_tag(
    uint8_t output[16],
    const uint8_t key[32],
    const char *label,
    const uint8_t *data,
    size_t data_length
)
{
    c0_hmac_sha256_ctx hmac;
    uint8_t digest[32];
    c0_hmac_sha256_init(&hmac, key, 32u);
    c0_hmac_sha256_update(
        &hmac,
        (const uint8_t *)label,
        strlen(label)
    );
    c0_hmac_sha256_update(&hmac, data, data_length);
    c0_hmac_sha256_final(&hmac, digest);
    memcpy(output, digest, 16u);
    c0pqlink_secure_zero(digest, sizeof(digest));
}

int c0pq_verify_auth_tag(
    const uint8_t tag[16],
    const uint8_t key[32],
    const char *label,
    const uint8_t *data,
    size_t data_length
)
{
    uint8_t expected[16];
    int result;
    c0pq_auth_tag(expected, key, label, data, data_length);
    result = c0pqlink_ct_equal(expected, tag, sizeof(expected))
        ? C0PQLINK_OK : C0PQLINK_ERR_AUTH;
    c0pqlink_secure_zero(expected, sizeof(expected));
    return result;
}

static void extract_psk(
    uint8_t early_secret[32],
    const uint8_t psk[C0PQ_PSK_BYTES]
)
{
    c0_hkdf_sha256_extract(early_secret, NULL, 0u, psk, C0PQ_PSK_BYTES);
}

static int expand_text_label(
    uint8_t *output,
    size_t output_length,
    const uint8_t secret[32],
    const char *label,
    const uint8_t transcript_hash[32]
)
{
    uint8_t info[64];
    const size_t label_length = strlen(label);
    if (label_length + 1u + 32u > sizeof(info)) {
        return C0PQLINK_ERR_CAPACITY;
    }
    memcpy(info, label, label_length);
    info[label_length] = 0u;
    memcpy(info + label_length + 1u, transcript_hash, 32u);
    return c0_hkdf_sha256_expand(
        output,
        output_length,
        secret,
        info,
        label_length + 1u + 32u
    );
}

void c0pq_derive_hello_key(
    uint8_t output[32],
    const uint8_t psk[C0PQ_PSK_BYTES]
)
{
    static const uint8_t label[] = "C0PQ/1 hello key";
    uint8_t early_secret[32];
    extract_psk(early_secret, psk);
    (void)c0_hkdf_sha256_expand(
        output,
        32u,
        early_secret,
        label,
        sizeof(label) - 1u
    );
    c0pqlink_secure_zero(early_secret, sizeof(early_secret));
}

void c0pq_derive_session_auth_key(
    uint8_t output[32],
    const uint8_t psk[C0PQ_PSK_BYTES],
    uint16_t suite,
    uint64_t epoch,
    const uint8_t key_id[C0PQ_KEY_ID_BYTES],
    const uint8_t device_nonce[C0PQ_NONCE_BYTES],
    const uint8_t server_nonce[C0PQ_NONCE_BYTES]
)
{
    static const uint8_t label[] = "C0PQ/1 session auth";
    c0_hmac_sha256_ctx hmac;
    uint8_t early_secret[32];
    uint8_t encoded[10];
    extract_psk(early_secret, psk);
    c0pq_store16_be(encoded, suite);
    c0pq_store64_be(encoded + 2u, epoch);
    c0_hmac_sha256_init(&hmac, early_secret, sizeof(early_secret));
    c0_hmac_sha256_update(&hmac, label, sizeof(label) - 1u);
    c0_hmac_sha256_update(&hmac, encoded, sizeof(encoded));
    c0_hmac_sha256_update(&hmac, key_id, C0PQ_KEY_ID_BYTES);
    c0_hmac_sha256_update(&hmac, device_nonce, C0PQ_NONCE_BYTES);
    c0_hmac_sha256_update(&hmac, server_nonce, C0PQ_NONCE_BYTES);
    c0_hmac_sha256_final(&hmac, output);
    c0pqlink_secure_zero(early_secret, sizeof(early_secret));
}

void c0pq_transcript_init(c0_sha256_ctx *transcript)
{
    c0_sha256_init(transcript);
    c0_sha256_update(
        transcript,
        transcript_prefix,
        sizeof(transcript_prefix)
    );
}

void c0pq_transcript_add_hello(
    c0_sha256_ctx *transcript,
    const uint8_t hello_frame[C0PQ_HELLO_FRAME_BYTES]
)
{
    c0_sha256_update(transcript, hello_frame, C0PQ_HELLO_CORE_BYTES);
}

void c0pq_transcript_add_challenge(
    c0_sha256_ctx *transcript,
    const uint8_t challenge_frame[C0PQ_CHALLENGE_FRAME_BYTES]
)
{
    c0_sha256_update(
        transcript,
        challenge_frame,
        C0PQ_CHALLENGE_CORE_BYTES
    );
}

int c0pq_derive_session_keys(
    c0pq_session_keys *keys,
    const uint8_t psk[C0PQ_PSK_BYTES],
    const uint8_t kem_shared_secret[32],
    const uint8_t transcript_hash[32]
)
{
    static const uint8_t derived_label[] = "C0PQ/1 derived";
    uint8_t early_secret[32];
    uint8_t derived_secret[32];
    uint8_t handshake_secret[32];
    int result = C0PQLINK_OK;
    if (keys == NULL || psk == NULL || kem_shared_secret == NULL
        || transcript_hash == NULL) {
        return C0PQLINK_ERR_ARGUMENT;
    }
    extract_psk(early_secret, psk);
    result = c0_hkdf_sha256_expand(
        derived_secret,
        sizeof(derived_secret),
        early_secret,
        derived_label,
        sizeof(derived_label) - 1u
    );
    if (result == C0PQLINK_OK) {
        c0_hkdf_sha256_extract(
            handshake_secret,
            derived_secret,
            sizeof(derived_secret),
            kem_shared_secret,
            32u
        );
        result = expand_text_label(
            keys->device_finished_key,
            32u,
            handshake_secret,
            "C0PQ/1 device finished",
            transcript_hash
        );
    }
    if (result == C0PQLINK_OK) {
        result = expand_text_label(
            keys->server_finished_key,
            32u,
            handshake_secret,
            "C0PQ/1 server finished",
            transcript_hash
        );
    }
    if (result == C0PQLINK_OK) {
        result = expand_text_label(
            keys->client_traffic_key,
            16u,
            handshake_secret,
            "C0PQ/1 client traffic",
            transcript_hash
        );
    }
    if (result == C0PQLINK_OK) {
        result = expand_text_label(
            keys->server_traffic_key,
            16u,
            handshake_secret,
            "C0PQ/1 server traffic",
            transcript_hash
        );
    }
    if (result == C0PQLINK_OK) {
        result = expand_text_label(
            keys->client_nonce_base,
            16u,
            handshake_secret,
            "C0PQ/1 client nonce",
            transcript_hash
        );
    }
    if (result == C0PQLINK_OK) {
        result = expand_text_label(
            keys->server_nonce_base,
            16u,
            handshake_secret,
            "C0PQ/1 server nonce",
            transcript_hash
        );
    }
    if (result == C0PQLINK_OK) {
        result = expand_text_label(
            keys->client_chain_key,
            32u,
            handshake_secret,
            "C0PQ/1 client chain",
            transcript_hash
        );
    }
    if (result == C0PQLINK_OK) {
        result = expand_text_label(
            keys->server_chain_key,
            32u,
            handshake_secret,
            "C0PQ/1 server chain",
            transcript_hash
        );
    }
    if (result != C0PQLINK_OK) {
        c0pqlink_secure_zero(keys, sizeof(*keys));
    }
    c0pqlink_secure_zero(early_secret, sizeof(early_secret));
    c0pqlink_secure_zero(derived_secret, sizeof(derived_secret));
    c0pqlink_secure_zero(handshake_secret, sizeof(handshake_secret));
    return result;
}

void c0pq_finished_tag(
    uint8_t output[16],
    const uint8_t finished_key[32],
    uint8_t frame_type,
    uint32_t session_id,
    const uint8_t transcript_hash[32]
)
{
    uint8_t input[C0PQ_FRAME_HEADER_BYTES + 32u];
    (void)c0pq_encode_header(
        input,
        frame_type,
        0u,
        C0PQ_FINISHED_BYTES,
        session_id
    );
    memcpy(input + C0PQ_FRAME_HEADER_BYTES, transcript_hash, 32u);
    c0pq_auth_tag(
        output,
        finished_key,
        "C0PQ/1 finished tag",
        input,
        sizeof(input)
    );
}

void c0pq_compute_public_key_id(
    uint8_t output[C0PQ_KEY_ID_BYTES],
    const uint8_t public_key[800]
)
{
    uint8_t digest[32];
    c0_sha256(digest, public_key, 800u);
    memcpy(output, digest, C0PQ_KEY_ID_BYTES);
    c0pqlink_secure_zero(digest, sizeof(digest));
}
