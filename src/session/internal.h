#ifndef C0PQLINK_SESSION_INTERNAL_H
#define C0PQLINK_SESSION_INTERNAL_H

#include "c0pqlink/client.h"

void c0pq_store16_be(uint8_t output[2], uint16_t value);
void c0pq_store32_be(uint8_t output[4], uint32_t value);
void c0pq_store64_be(uint8_t output[8], uint64_t value);
uint16_t c0pq_load16_be(const uint8_t input[2]);
uint32_t c0pq_load32_be(const uint8_t input[4]);
uint64_t c0pq_load64_be(const uint8_t input[8]);

void c0pq_auth_tag(
    uint8_t output[C0PQ_AUTH_TAG_BYTES],
    const uint8_t key[32],
    const char *label,
    const uint8_t *data,
    size_t data_length
);

int c0pq_verify_auth_tag(
    const uint8_t tag[C0PQ_AUTH_TAG_BYTES],
    const uint8_t key[32],
    const char *label,
    const uint8_t *data,
    size_t data_length
);

void c0pq_ratchet_step(
    const uint8_t chain_key[32],
    uint8_t message_key[16],
    uint8_t next_chain_key[32]
);

void c0pq_make_record_nonce(
    uint8_t nonce[16],
    const uint8_t base[16],
    uint64_t sequence
);

#endif
