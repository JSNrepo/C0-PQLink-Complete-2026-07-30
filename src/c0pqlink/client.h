#ifndef C0PQLINK_CLIENT_H
#define C0PQLINK_CLIENT_H

#include "c0pqlink/ascon_aead128.h"
#include "c0pqlink/mlkem512_stream.h"
#include "c0pqlink/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    C0PQ_FULL_PQ_EACH_SESSION = 0,
    C0PQ_PQ_BOOTSTRAP_RATCHET = 1
} c0pq_session_mode;

typedef enum {
    C0PQ_CLIENT_UNINITIALIZED = 0,
    C0PQ_CLIENT_READY = 1,
    C0PQ_CLIENT_CONNECTING = 2,
    C0PQ_CLIENT_ESTABLISHED = 3,
    C0PQ_CLIENT_FAILED = 4
} c0pq_client_state;

typedef int (*c0pq_transport_send_fn)(
    void *context,
    const uint8_t *frame,
    size_t frame_length
);

typedef int (*c0pq_transport_receive_fn)(
    void *context,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *frame_length,
    uint32_t timeout_ms
);

typedef struct {
    uint8_t device_id[C0PQ_DEVICE_ID_BYTES];
    uint8_t psk[C0PQ_PSK_BYTES];
    uint64_t epoch;
    uint8_t public_key_id[C0PQ_KEY_ID_BYTES];
    c0_mlkem512_pk_read_fn read_public_key;
    void *public_key_context;
    c0pqlink_rng_fn random_bytes;
    void *rng_context;
    c0pq_transport_send_fn send_frame;
    c0pq_transport_receive_fn receive_frame;
    void *transport_context;
    c0pq_session_mode mode;
    uint32_t timeout_ms;
    uint8_t maximum_retries;
} c0pq_client_config;

typedef struct {
    c0pq_client_config config;
    c0pq_client_state state;
    uint32_t session_id;
    /*
     * In FULL_PQ_EACH_SESSION only the first 16 bytes of each secret are
     * active traffic keys. In PQ_BOOTSTRAP_RATCHET all 32 bytes are chain
     * keys. Handshake-only Finished keys never remain in the live context.
     */
    uint8_t send_secret[32];
    uint8_t receive_secret[32];
    uint8_t send_nonce_base[16];
    uint8_t receive_nonce_base[16];
    uint64_t send_sequence;
    uint64_t receive_sequence;
} c0pq_client;

int c0pq_client_init(
    c0pq_client *client,
    const c0pq_client_config *config
);

int c0pq_client_connect(
    c0pq_client *client,
    c0_mlkem512_workspace *workspace
);

int c0pq_client_seal_record(
    c0pq_client *client,
    const uint8_t *plaintext,
    size_t plaintext_length,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *frame_length
);

/*
 * A successful seal advances the sending sequence/ratchet exactly once.
 * On packet loss, retain and retransmit the exact returned frame. Never call
 * seal again to recreate the same logical record. Reliable one-way delivery
 * requires an application/transport acknowledgment.
 */

int c0pq_client_open_record(
    c0pq_client *client,
    const uint8_t *frame,
    size_t frame_length,
    uint8_t *plaintext,
    size_t plaintext_capacity,
    size_t *plaintext_length
);

void c0pq_client_close(c0pq_client *client);

#ifdef __cplusplus
}
#endif

#endif
