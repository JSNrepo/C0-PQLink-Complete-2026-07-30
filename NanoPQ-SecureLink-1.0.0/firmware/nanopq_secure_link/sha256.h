#ifndef QP_SHA256_H
#define QP_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef union {
  uint8_t bytes[64];
  uint32_t words[16];
} qp_sha256_block;

typedef struct {
  qp_sha256_block block;
  uint32_t state[8];
  uint32_t total_bytes;
  uint8_t block_len;
} qp_sha256_ctx;

void qp_sha256_init(qp_sha256_ctx *ctx);
void qp_sha256_update(qp_sha256_ctx *ctx, const uint8_t *data, size_t len);
void qp_sha256_final(qp_sha256_ctx *ctx, uint8_t digest[32]);
void qp_sha256_final_16(qp_sha256_ctx *ctx, uint8_t digest[16]);
void qp_sha256_fips205_seed(uint32_t state[8],
                            const uint8_t pk_seed[16]);
void qp_sha256(const uint8_t *data, size_t len, uint8_t digest[32]);

/*
 * HMAC-SHA-256 with a fixed 256-bit key.  The project uses a full 32-byte
 * authentication tag; callers must not truncate it.
 */
void qp_hmac_sha256_32(const uint8_t key[32], const uint8_t *message,
                       size_t message_len, uint8_t tag[32]);

int qp_constant_time_equal(const uint8_t *a, const uint8_t *b, size_t len);

#endif
