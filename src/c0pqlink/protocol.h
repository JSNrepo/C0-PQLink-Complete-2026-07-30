#ifndef C0PQLINK_PROTOCOL_H
#define C0PQLINK_PROTOCOL_H

#include "c0pqlink/common.h"
#include "c0pqlink/sha256.h"

#ifdef __cplusplus
extern "C" {
#endif

#define C0PQ_WIRE_VERSION 1u
#define C0PQ_SUITE_MLKEM512_HKDFSHA256_ASCON128 0x0001u

#define C0PQ_DEVICE_ID_BYTES 8u
#define C0PQ_PSK_BYTES 32u
#define C0PQ_NONCE_BYTES 16u
#define C0PQ_KEY_ID_BYTES 16u
#define C0PQ_AUTH_TAG_BYTES 16u
#define C0PQ_FINISHED_BYTES 16u
#define C0PQ_FRAME_HEADER_BYTES 10u
#define C0PQ_FRAME_MAX_BYTES 96u
#define C0PQ_FRAGMENT_PAYLOAD_BYTES 48u
#define C0PQ_FRAGMENT_COUNT 16u
#define C0PQ_RECORD_PLAINTEXT_MAX 48u

#define C0PQ_HELLO_CORE_BYTES 44u
#define C0PQ_HELLO_FRAME_BYTES 60u
#define C0PQ_CHALLENGE_CORE_BYTES 68u
#define C0PQ_CHALLENGE_FRAME_BYTES 84u
#define C0PQ_FRAGMENT_OVERHEAD_BYTES 29u
#define C0PQ_ACK_FRAME_BYTES 28u
#define C0PQ_FINISHED_FRAME_BYTES 26u

typedef enum {
    C0PQ_FRAME_HELLO = 1,
    C0PQ_FRAME_CHALLENGE = 2,
    C0PQ_FRAME_CIPHERTEXT_FRAGMENT = 3,
    C0PQ_FRAME_CIPHERTEXT_ACK = 4,
    C0PQ_FRAME_DEVICE_FINISHED = 5,
    C0PQ_FRAME_SERVER_FINISHED = 6,
    C0PQ_FRAME_DATA = 7,
    C0PQ_FRAME_ERROR = 127
} c0pq_frame_type;

typedef struct {
    uint8_t type;
    uint8_t flags;
    uint16_t payload_length;
    uint32_t session_id;
} c0pq_frame_header;

typedef struct {
    uint16_t suite;
    uint8_t device_id[C0PQ_DEVICE_ID_BYTES];
    uint64_t epoch;
    uint8_t device_nonce[C0PQ_NONCE_BYTES];
} c0pq_hello;

typedef struct {
    uint16_t suite;
    uint64_t epoch;
    uint8_t key_id[C0PQ_KEY_ID_BYTES];
    uint8_t device_nonce[C0PQ_NONCE_BYTES];
    uint8_t server_nonce[C0PQ_NONCE_BYTES];
} c0pq_challenge;

typedef struct {
    uint8_t hello_key[32];
    uint8_t session_auth_key[32];
} c0pq_preflight_keys;

typedef struct {
    uint8_t device_finished_key[32];
    uint8_t server_finished_key[32];
    uint8_t client_traffic_key[16];
    uint8_t server_traffic_key[16];
    uint8_t client_nonce_base[16];
    uint8_t server_nonce_base[16];
    uint8_t client_chain_key[32];
    uint8_t server_chain_key[32];
} c0pq_session_keys;

int c0pq_encode_header(
    uint8_t output[C0PQ_FRAME_HEADER_BYTES],
    uint8_t type,
    uint8_t flags,
    uint16_t payload_length,
    uint32_t session_id
);

int c0pq_decode_header(
    c0pq_frame_header *header,
    const uint8_t *frame,
    size_t frame_length
);

void c0pq_derive_hello_key(
    uint8_t output[32],
    const uint8_t psk[C0PQ_PSK_BYTES]
);

void c0pq_derive_session_auth_key(
    uint8_t output[32],
    const uint8_t psk[C0PQ_PSK_BYTES],
    uint16_t suite,
    uint64_t epoch,
    const uint8_t key_id[C0PQ_KEY_ID_BYTES],
    const uint8_t device_nonce[C0PQ_NONCE_BYTES],
    const uint8_t server_nonce[C0PQ_NONCE_BYTES]
);

int c0pq_encode_hello(
    uint8_t output[C0PQ_HELLO_FRAME_BYTES],
    uint32_t session_id,
    const c0pq_hello *hello,
    const uint8_t psk[C0PQ_PSK_BYTES]
);

int c0pq_decode_verify_hello(
    c0pq_hello *hello,
    uint32_t *session_id,
    const uint8_t *frame,
    size_t frame_length,
    const uint8_t psk[C0PQ_PSK_BYTES]
);

int c0pq_encode_challenge(
    uint8_t output[C0PQ_CHALLENGE_FRAME_BYTES],
    uint32_t session_id,
    const c0pq_challenge *challenge,
    const uint8_t psk[C0PQ_PSK_BYTES]
);

int c0pq_decode_verify_challenge(
    c0pq_challenge *challenge,
    uint32_t expected_session_id,
    const uint8_t *frame,
    size_t frame_length,
    const uint8_t psk[C0PQ_PSK_BYTES]
);

void c0pq_transcript_init(c0_sha256_ctx *transcript);
void c0pq_transcript_add_hello(
    c0_sha256_ctx *transcript,
    const uint8_t hello_frame[C0PQ_HELLO_FRAME_BYTES]
);
void c0pq_transcript_add_challenge(
    c0_sha256_ctx *transcript,
    const uint8_t challenge_frame[C0PQ_CHALLENGE_FRAME_BYTES]
);

int c0pq_derive_session_keys(
    c0pq_session_keys *keys,
    const uint8_t psk[C0PQ_PSK_BYTES],
    const uint8_t kem_shared_secret[32],
    const uint8_t transcript_hash[32]
);

void c0pq_finished_tag(
    uint8_t output[C0PQ_FINISHED_BYTES],
    const uint8_t finished_key[32],
    uint8_t frame_type,
    uint32_t session_id,
    const uint8_t transcript_hash[32]
);

void c0pq_compute_public_key_id(
    uint8_t output[C0PQ_KEY_ID_BYTES],
    const uint8_t public_key[800]
);

#ifdef __cplusplus
}
#endif

#endif
