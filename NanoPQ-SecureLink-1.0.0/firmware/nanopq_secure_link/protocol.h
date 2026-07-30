#ifndef NPQ_PROTOCOL_H
#define NPQ_PROTOCOL_H

#include "common.h"

#define NPQ_ROOT_KEY_BYTES 32u
#define NPQ_DEVICE_ID_BYTES 8u
#define NPQ_NONCE_BYTES 16u
#define NPQ_TAG_BYTES 16u
#define NPQ_CHAIN_KEY_BYTES 32u
#define NPQ_FRAME_MAX_BYTES 64u
#define NPQ_DATA_MAX_BYTES 24u

#define NPQ_HELLO_BYTES 48u
#define NPQ_CHALLENGE_BYTES 60u
#define NPQ_FINISHED_BYTES 24u
#define NPQ_DATA_OVERHEAD_BYTES 29u

#define NPQ_FRAME_HELLO 1u
#define NPQ_FRAME_CHALLENGE 2u
#define NPQ_FRAME_CLIENT_FINISHED 3u
#define NPQ_FRAME_SERVER_FINISHED 4u
#define NPQ_FRAME_DATA 5u
#define NPQ_FRAME_ENROLL_REQUIRED 6u
#define NPQ_FRAME_ENROLL_BEGIN 7u
#define NPQ_FRAME_ENROLL_RESULT 8u
#define NPQ_FRAME_ENROLL_CHUNK_READY 9u

#define NPQ_ROLE_DEVICE 0u
#define NPQ_ROLE_SERVER 1u

typedef struct {
    uint8_t send_chain[NPQ_CHAIN_KEY_BYTES];
    uint8_t receive_chain[NPQ_CHAIN_KEY_BYTES];
    uint8_t send_nonce_base[NPQ_NONCE_BYTES];
    uint8_t receive_nonce_base[NPQ_NONCE_BYTES];
    uint32_t session_id;
    uint32_t boot_epoch;
    uint32_t send_sequence;
    uint32_t receive_sequence;
    uint8_t established;
} npq_session;

int npq_make_hello(
    uint8_t output[NPQ_HELLO_BYTES],
    const uint8_t root_key[NPQ_ROOT_KEY_BYTES],
    const uint8_t device_id[NPQ_DEVICE_ID_BYTES],
    uint32_t boot_epoch
);

int npq_verify_hello(
    const uint8_t frame[NPQ_HELLO_BYTES],
    const uint8_t root_key[NPQ_ROOT_KEY_BYTES]
);

int npq_make_challenge(
    uint8_t output[NPQ_CHALLENGE_BYTES],
    const uint8_t root_key[NPQ_ROOT_KEY_BYTES],
    const uint8_t hello[NPQ_HELLO_BYTES],
    const uint8_t server_nonce[NPQ_NONCE_BYTES]
);

int npq_verify_challenge(
    const uint8_t frame[NPQ_CHALLENGE_BYTES],
    const uint8_t root_key[NPQ_ROOT_KEY_BYTES],
    const uint8_t hello[NPQ_HELLO_BYTES]
);

int npq_derive_session(
    npq_session *session,
    uint8_t role,
    const uint8_t root_key[NPQ_ROOT_KEY_BYTES],
    const uint8_t hello[NPQ_HELLO_BYTES],
    const uint8_t challenge[NPQ_CHALLENGE_BYTES]
);

int npq_make_finished(
    uint8_t output[NPQ_FINISHED_BYTES],
    uint8_t frame_type,
    uint8_t role,
    const uint8_t root_key[NPQ_ROOT_KEY_BYTES],
    const uint8_t hello[NPQ_HELLO_BYTES],
    const uint8_t challenge[NPQ_CHALLENGE_BYTES]
);

int npq_verify_finished(
    const uint8_t frame[NPQ_FINISHED_BYTES],
    uint8_t expected_type,
    uint8_t role,
    const uint8_t root_key[NPQ_ROOT_KEY_BYTES],
    const uint8_t hello[NPQ_HELLO_BYTES],
    const uint8_t challenge[NPQ_CHALLENGE_BYTES]
);

int npq_seal(
    npq_session *session,
    const uint8_t *plaintext,
    uint8_t plaintext_length,
    uint8_t output[NPQ_FRAME_MAX_BYTES],
    uint8_t *output_length
);

int npq_open(
    npq_session *session,
    const uint8_t *frame,
    uint8_t frame_length,
    uint8_t plaintext[NPQ_DATA_MAX_BYTES],
    uint8_t *plaintext_length
);

void npq_close(npq_session *session);

#endif
