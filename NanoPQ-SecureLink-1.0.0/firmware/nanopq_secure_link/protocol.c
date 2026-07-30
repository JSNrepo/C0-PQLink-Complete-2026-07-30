#include "protocol.h"

#include "ascon_aead128.h"
#include "sha256.h"

#include <string.h>

#define NPQ_VERSION 1u
#define NPQ_HEADER_BYTES 4u
#define NPQ_HELLO_CORE_BYTES 32u
#define NPQ_CHALLENGE_CORE_BYTES 44u
#define NPQ_FINISHED_CORE_BYTES 8u
#define NPQ_DATA_CORE_BYTES 13u
#define NPQ_LABEL_MAX_BYTES 24u

static const uint8_t npq_magic[2] = { 0x4eu, 0x51u };

static int valid_header(const uint8_t *frame, uint8_t type)
{
    return frame[0] == npq_magic[0]
        && frame[1] == npq_magic[1]
        && frame[2] == NPQ_VERSION
        && frame[3] == type;
}

static void make_header(uint8_t *frame, uint8_t type)
{
    frame[0] = npq_magic[0];
    frame[1] = npq_magic[1];
    frame[2] = NPQ_VERSION;
    frame[3] = type;
}

static int hmac_label(
    uint8_t output[32],
    const uint8_t key[32],
    const char *label,
    const uint8_t *data,
    uint8_t data_length
)
{
    uint8_t input[NPQ_LABEL_MAX_BYTES + 1u + NPQ_FRAME_MAX_BYTES];
    const size_t label_length = strlen(label);
    if (label_length > NPQ_LABEL_MAX_BYTES
        || data_length > NPQ_FRAME_MAX_BYTES) {
        return NPQ_ERR_CAPACITY;
    }
    memcpy(input, label, label_length);
    input[label_length] = 0u;
    if (data_length != 0u) {
        memcpy(input + label_length + 1u, data, data_length);
    }
    qp_hmac_sha256_32(
        key,
        input,
        label_length + 1u + data_length,
        output
    );
    npq_secure_zero(input, sizeof(input));
    return NPQ_OK;
}

static int verify_mac16(
    const uint8_t tag[16],
    const uint8_t key[32],
    const char *label,
    const uint8_t *data,
    uint8_t data_length
)
{
    uint8_t digest[32];
    int result = hmac_label(digest, key, label, data, data_length);
    if (result == NPQ_OK
        && !npq_constant_time_equal(tag, digest, NPQ_TAG_BYTES)) {
        result = NPQ_ERR_AUTH;
    }
    npq_secure_zero(digest, sizeof(digest));
    return result;
}

static void transcript_hash(
    uint8_t output[32],
    const uint8_t hello[NPQ_HELLO_BYTES],
    const uint8_t challenge[NPQ_CHALLENGE_BYTES]
)
{
    qp_sha256_ctx hash;
    static const uint8_t domain[] = "NPQ/1 transcript";
    qp_sha256_init(&hash);
    qp_sha256_update(&hash, domain, sizeof(domain) - 1u);
    qp_sha256_update(&hash, hello, NPQ_HELLO_BYTES);
    qp_sha256_update(&hash, challenge, NPQ_CHALLENGE_BYTES);
    qp_sha256_final(&hash, output);
}

static void session_prk(
    uint8_t prk[32],
    const uint8_t root_key[32],
    const uint8_t transcript[32]
)
{
    qp_hmac_sha256_32(transcript, root_key, 32u, prk);
}

static int hkdf_expand_label(
    uint8_t output[32],
    const uint8_t prk[32],
    const char *label,
    const uint8_t transcript[32]
)
{
    uint8_t input[NPQ_LABEL_MAX_BYTES + 1u + 32u + 1u];
    const size_t label_length = strlen(label);
    if (label_length > NPQ_LABEL_MAX_BYTES) {
        return NPQ_ERR_CAPACITY;
    }
    memcpy(input, label, label_length);
    input[label_length] = 0u;
    memcpy(input + label_length + 1u, transcript, 32u);
    input[label_length + 1u + 32u] = 1u;
    qp_hmac_sha256_32(
        prk,
        input,
        label_length + 1u + 32u + 1u,
        output
    );
    npq_secure_zero(input, sizeof(input));
    return NPQ_OK;
}

static int derive_transcript_prk(
    uint8_t transcript[32],
    uint8_t prk[32],
    const uint8_t root_key[32],
    const uint8_t hello[NPQ_HELLO_BYTES],
    const uint8_t challenge[NPQ_CHALLENGE_BYTES]
)
{
    int result = npq_verify_hello(hello, root_key);
    if (result == NPQ_OK) {
        result = npq_verify_challenge(challenge, root_key, hello);
    }
    if (result != NPQ_OK) {
        return result;
    }
    transcript_hash(transcript, hello, challenge);
    session_prk(prk, root_key, transcript);
    return NPQ_OK;
}

int npq_make_hello(
    uint8_t output[NPQ_HELLO_BYTES],
    const uint8_t root_key[NPQ_ROOT_KEY_BYTES],
    const uint8_t device_id[NPQ_DEVICE_ID_BYTES],
    uint32_t boot_epoch
)
{
    uint8_t digest[32];
    uint8_t nonce_input[NPQ_DEVICE_ID_BYTES + 4u];
    if (output == NULL || root_key == NULL || device_id == NULL
        || boot_epoch == 0u) {
        return NPQ_ERR_ARGUMENT;
    }
    make_header(output, NPQ_FRAME_HELLO);
    memcpy(output + 4u, device_id, NPQ_DEVICE_ID_BYTES);
    npq_store_u32_be(output + 12u, boot_epoch);
    memcpy(nonce_input, device_id, NPQ_DEVICE_ID_BYTES);
    npq_store_u32_be(nonce_input + NPQ_DEVICE_ID_BYTES, boot_epoch);
    (void)hmac_label(
        digest,
        root_key,
        "NPQ/1 client nonce",
        nonce_input,
        sizeof(nonce_input)
    );
    memcpy(output + 16u, digest, NPQ_NONCE_BYTES);
    (void)hmac_label(
        digest,
        root_key,
        "NPQ/1 hello",
        output,
        NPQ_HELLO_CORE_BYTES
    );
    memcpy(output + NPQ_HELLO_CORE_BYTES, digest, NPQ_TAG_BYTES);
    npq_secure_zero(digest, sizeof(digest));
    npq_secure_zero(nonce_input, sizeof(nonce_input));
    return NPQ_OK;
}

int npq_verify_hello(
    const uint8_t frame[NPQ_HELLO_BYTES],
    const uint8_t root_key[NPQ_ROOT_KEY_BYTES]
)
{
    if (frame == NULL || root_key == NULL
        || !valid_header(frame, NPQ_FRAME_HELLO)
        || npq_load_u32_be(frame + 12u) == 0u) {
        return NPQ_ERR_FORMAT;
    }
    return verify_mac16(
        frame + NPQ_HELLO_CORE_BYTES,
        root_key,
        "NPQ/1 hello",
        frame,
        NPQ_HELLO_CORE_BYTES
    );
}

int npq_make_challenge(
    uint8_t output[NPQ_CHALLENGE_BYTES],
    const uint8_t root_key[NPQ_ROOT_KEY_BYTES],
    const uint8_t hello[NPQ_HELLO_BYTES],
    const uint8_t server_nonce[NPQ_NONCE_BYTES]
)
{
    uint8_t digest[32];
    int result;
    if (output == NULL || server_nonce == NULL) {
        return NPQ_ERR_ARGUMENT;
    }
    result = npq_verify_hello(hello, root_key);
    if (result != NPQ_OK) {
        return result;
    }
    make_header(output, NPQ_FRAME_CHALLENGE);
    memcpy(output + 4u, hello + 12u, 4u);
    memcpy(output + 8u, hello + 16u, NPQ_NONCE_BYTES);
    memcpy(output + 24u, server_nonce, NPQ_NONCE_BYTES);
    (void)hmac_label(
        digest,
        root_key,
        "NPQ/1 session id",
        output,
        40u
    );
    memcpy(output + 40u, digest, 4u);
    (void)hmac_label(
        digest,
        root_key,
        "NPQ/1 challenge",
        output,
        NPQ_CHALLENGE_CORE_BYTES
    );
    memcpy(output + NPQ_CHALLENGE_CORE_BYTES, digest, NPQ_TAG_BYTES);
    npq_secure_zero(digest, sizeof(digest));
    return NPQ_OK;
}

int npq_verify_challenge(
    const uint8_t frame[NPQ_CHALLENGE_BYTES],
    const uint8_t root_key[NPQ_ROOT_KEY_BYTES],
    const uint8_t hello[NPQ_HELLO_BYTES]
)
{
    int result;
    if (frame == NULL || root_key == NULL || hello == NULL
        || !valid_header(frame, NPQ_FRAME_CHALLENGE)
        || !npq_constant_time_equal(frame + 4u, hello + 12u, 4u)
        || !npq_constant_time_equal(
            frame + 8u,
            hello + 16u,
            NPQ_NONCE_BYTES
        )) {
        return NPQ_ERR_FORMAT;
    }
    result = verify_mac16(
        frame + NPQ_CHALLENGE_CORE_BYTES,
        root_key,
        "NPQ/1 challenge",
        frame,
        NPQ_CHALLENGE_CORE_BYTES
    );
    return result;
}

int npq_derive_session(
    npq_session *session,
    uint8_t role,
    const uint8_t root_key[NPQ_ROOT_KEY_BYTES],
    const uint8_t hello[NPQ_HELLO_BYTES],
    const uint8_t challenge[NPQ_CHALLENGE_BYTES]
)
{
    uint8_t transcript[32];
    uint8_t prk[32];
    uint8_t material[32];
    uint8_t client_chain[32];
    uint8_t server_chain[32];
    uint8_t client_nonce[16];
    uint8_t server_nonce[16];
    int result;
    if (session == NULL || (role != NPQ_ROLE_DEVICE
        && role != NPQ_ROLE_SERVER)) {
        return NPQ_ERR_ARGUMENT;
    }
    result = derive_transcript_prk(
        transcript,
        prk,
        root_key,
        hello,
        challenge
    );
    if (result != NPQ_OK) {
        return result;
    }
    (void)hkdf_expand_label(
        client_chain,
        prk,
        "NPQ/1 client chain",
        transcript
    );
    (void)hkdf_expand_label(
        server_chain,
        prk,
        "NPQ/1 server chain",
        transcript
    );
    (void)hkdf_expand_label(
        material,
        prk,
        "NPQ/1 client nonce",
        transcript
    );
    memcpy(client_nonce, material, sizeof(client_nonce));
    (void)hkdf_expand_label(
        material,
        prk,
        "NPQ/1 server nonce",
        transcript
    );
    memcpy(server_nonce, material, sizeof(server_nonce));
    memset(session, 0, sizeof(*session));
    if (role == NPQ_ROLE_DEVICE) {
        memcpy(session->send_chain, client_chain, sizeof(client_chain));
        memcpy(session->receive_chain, server_chain, sizeof(server_chain));
        memcpy(session->send_nonce_base, client_nonce, sizeof(client_nonce));
        memcpy(session->receive_nonce_base, server_nonce, sizeof(server_nonce));
    } else {
        memcpy(session->send_chain, server_chain, sizeof(server_chain));
        memcpy(session->receive_chain, client_chain, sizeof(client_chain));
        memcpy(session->send_nonce_base, server_nonce, sizeof(server_nonce));
        memcpy(session->receive_nonce_base, client_nonce, sizeof(client_nonce));
    }
    session->session_id = npq_load_u32_be(challenge + 40u);
    session->boot_epoch = npq_load_u32_be(hello + 12u);
    session->established = 1u;
    npq_secure_zero(transcript, sizeof(transcript));
    npq_secure_zero(prk, sizeof(prk));
    npq_secure_zero(material, sizeof(material));
    npq_secure_zero(client_chain, sizeof(client_chain));
    npq_secure_zero(server_chain, sizeof(server_chain));
    npq_secure_zero(client_nonce, sizeof(client_nonce));
    npq_secure_zero(server_nonce, sizeof(server_nonce));
    return NPQ_OK;
}

static const char *finished_key_label(uint8_t frame_type)
{
    return frame_type == NPQ_FRAME_CLIENT_FINISHED
        ? "NPQ/1 client finished"
        : "NPQ/1 server finished";
}

static const char *finished_tag_label(uint8_t frame_type)
{
    return frame_type == NPQ_FRAME_CLIENT_FINISHED
        ? "NPQ/1 client proof"
        : "NPQ/1 server proof";
}

int npq_make_finished(
    uint8_t output[NPQ_FINISHED_BYTES],
    uint8_t frame_type,
    uint8_t role,
    const uint8_t root_key[NPQ_ROOT_KEY_BYTES],
    const uint8_t hello[NPQ_HELLO_BYTES],
    const uint8_t challenge[NPQ_CHALLENGE_BYTES]
)
{
    uint8_t transcript[32];
    uint8_t prk[32];
    uint8_t key[32];
    uint8_t proof_input[NPQ_FINISHED_CORE_BYTES + 32u];
    uint8_t digest[32];
    int result;
    (void)role;
    if (output == NULL
        || (frame_type != NPQ_FRAME_CLIENT_FINISHED
            && frame_type != NPQ_FRAME_SERVER_FINISHED)) {
        return NPQ_ERR_ARGUMENT;
    }
    result = derive_transcript_prk(
        transcript,
        prk,
        root_key,
        hello,
        challenge
    );
    if (result != NPQ_OK) {
        return result;
    }
    (void)hkdf_expand_label(
        key,
        prk,
        finished_key_label(frame_type),
        transcript
    );
    make_header(output, frame_type);
    memcpy(output + 4u, challenge + 40u, 4u);
    memcpy(proof_input, output, NPQ_FINISHED_CORE_BYTES);
    memcpy(proof_input + NPQ_FINISHED_CORE_BYTES, transcript, 32u);
    (void)hmac_label(
        digest,
        key,
        finished_tag_label(frame_type),
        proof_input,
        sizeof(proof_input)
    );
    memcpy(output + NPQ_FINISHED_CORE_BYTES, digest, NPQ_TAG_BYTES);
    npq_secure_zero(transcript, sizeof(transcript));
    npq_secure_zero(prk, sizeof(prk));
    npq_secure_zero(key, sizeof(key));
    npq_secure_zero(proof_input, sizeof(proof_input));
    npq_secure_zero(digest, sizeof(digest));
    return NPQ_OK;
}

int npq_verify_finished(
    const uint8_t frame[NPQ_FINISHED_BYTES],
    uint8_t expected_type,
    uint8_t role,
    const uint8_t root_key[NPQ_ROOT_KEY_BYTES],
    const uint8_t hello[NPQ_HELLO_BYTES],
    const uint8_t challenge[NPQ_CHALLENGE_BYTES]
)
{
    uint8_t expected[NPQ_FINISHED_BYTES];
    int result;
    if (frame == NULL || !valid_header(frame, expected_type)
        || !npq_constant_time_equal(frame + 4u, challenge + 40u, 4u)) {
        return NPQ_ERR_FORMAT;
    }
    result = npq_make_finished(
        expected,
        expected_type,
        role,
        root_key,
        hello,
        challenge
    );
    if (result == NPQ_OK
        && !npq_constant_time_equal(
            frame + NPQ_FINISHED_CORE_BYTES,
            expected + NPQ_FINISHED_CORE_BYTES,
            NPQ_TAG_BYTES
        )) {
        result = NPQ_ERR_AUTH;
    }
    npq_secure_zero(expected, sizeof(expected));
    return result;
}

static void ratchet_material(
    const uint8_t chain[32],
    uint32_t sequence,
    uint8_t message_key[16],
    uint8_t next_chain[32]
)
{
    uint8_t encoded[4];
    uint8_t digest[32];
    npq_store_u32_be(encoded, sequence);
    (void)hmac_label(
        digest,
        chain,
        "NPQ/1 message key",
        encoded,
        sizeof(encoded)
    );
    memcpy(message_key, digest, 16u);
    (void)hmac_label(
        next_chain,
        chain,
        "NPQ/1 next chain",
        encoded,
        sizeof(encoded)
    );
    npq_secure_zero(encoded, sizeof(encoded));
    npq_secure_zero(digest, sizeof(digest));
}

static void make_nonce(
    uint8_t nonce[16],
    const uint8_t base[16],
    uint32_t sequence
)
{
    uint8_t encoded[4];
    uint8_t index;
    memcpy(nonce, base, 16u);
    npq_store_u32_be(encoded, sequence);
    for (index = 0u; index < 4u; ++index) {
        nonce[12u + index] ^= encoded[index];
    }
    npq_secure_zero(encoded, sizeof(encoded));
}

int npq_seal(
    npq_session *session,
    const uint8_t *plaintext,
    uint8_t plaintext_length,
    uint8_t output[NPQ_FRAME_MAX_BYTES],
    uint8_t *output_length
)
{
    uint8_t message_key[16];
    uint8_t next_chain[32];
    uint8_t nonce[16];
    if (session == NULL || output == NULL || output_length == NULL
        || (plaintext == NULL && plaintext_length != 0u)
        || plaintext_length > NPQ_DATA_MAX_BYTES) {
        return NPQ_ERR_ARGUMENT;
    }
    if (session->established == 0u
        || session->send_sequence == UINT32_MAX) {
        return NPQ_ERR_STATE;
    }
    make_header(output, NPQ_FRAME_DATA);
    npq_store_u32_be(output + 4u, session->session_id);
    npq_store_u32_be(output + 8u, session->send_sequence);
    output[12] = plaintext_length;
    ratchet_material(
        session->send_chain,
        session->send_sequence,
        message_key,
        next_chain
    );
    make_nonce(nonce, session->send_nonce_base, session->send_sequence);
    npq_ascon_aead128_encrypt(
        output + NPQ_DATA_CORE_BYTES,
        output + NPQ_DATA_CORE_BYTES + plaintext_length,
        message_key,
        nonce,
        output,
        NPQ_DATA_CORE_BYTES,
        plaintext,
        plaintext_length
    );
    memcpy(session->send_chain, next_chain, sizeof(next_chain));
    ++session->send_sequence;
    *output_length = (uint8_t)(
        NPQ_DATA_OVERHEAD_BYTES + plaintext_length
    );
    npq_secure_zero(message_key, sizeof(message_key));
    npq_secure_zero(next_chain, sizeof(next_chain));
    npq_secure_zero(nonce, sizeof(nonce));
    return NPQ_OK;
}

int npq_open(
    npq_session *session,
    const uint8_t *frame,
    uint8_t frame_length,
    uint8_t plaintext[NPQ_DATA_MAX_BYTES],
    uint8_t *plaintext_length
)
{
    uint8_t message_key[16];
    uint8_t next_chain[32];
    uint8_t nonce[16];
    uint8_t encoded_length;
    uint32_t sequence;
    int result;
    if (session == NULL || frame == NULL || plaintext == NULL
        || plaintext_length == NULL
        || frame_length < NPQ_DATA_OVERHEAD_BYTES
        || !valid_header(frame, NPQ_FRAME_DATA)) {
        return NPQ_ERR_ARGUMENT;
    }
    encoded_length = frame[12];
    if (encoded_length > NPQ_DATA_MAX_BYTES
        || frame_length != NPQ_DATA_OVERHEAD_BYTES + encoded_length
        || npq_load_u32_be(frame + 4u) != session->session_id) {
        return NPQ_ERR_FORMAT;
    }
    sequence = npq_load_u32_be(frame + 8u);
    if (session->established == 0u
        || sequence != session->receive_sequence) {
        return NPQ_ERR_REPLAY;
    }
    ratchet_material(
        session->receive_chain,
        sequence,
        message_key,
        next_chain
    );
    make_nonce(nonce, session->receive_nonce_base, sequence);
    result = npq_ascon_aead128_decrypt(
        plaintext,
        message_key,
        nonce,
        frame,
        NPQ_DATA_CORE_BYTES,
        frame + NPQ_DATA_CORE_BYTES,
        encoded_length,
        frame + NPQ_DATA_CORE_BYTES + encoded_length
    );
    if (result == NPQ_OK) {
        memcpy(session->receive_chain, next_chain, sizeof(next_chain));
        ++session->receive_sequence;
        *plaintext_length = encoded_length;
    } else {
        *plaintext_length = 0u;
    }
    npq_secure_zero(message_key, sizeof(message_key));
    npq_secure_zero(next_chain, sizeof(next_chain));
    npq_secure_zero(nonce, sizeof(nonce));
    return result;
}

void npq_close(npq_session *session)
{
    if (session != NULL) {
        npq_secure_zero(session, sizeof(*session));
    }
}
