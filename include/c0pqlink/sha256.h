#ifndef C0PQLINK_SHA256_H
#define C0PQLINK_SHA256_H

#include "c0pqlink/common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define C0_SHA256_BYTES 32u
#define C0_HMAC_SHA256_BYTES 32u

typedef struct {
    uint32_t state[8];
    uint64_t total_bytes;
    uint8_t buffer[64];
    uint8_t buffered;
} c0_sha256_ctx;

typedef struct {
    c0_sha256_ctx inner;
    uint8_t outer_pad[64];
} c0_hmac_sha256_ctx;

void c0_sha256_init(c0_sha256_ctx *ctx);
void c0_sha256_update(
    c0_sha256_ctx *ctx,
    const uint8_t *input,
    size_t length
);
void c0_sha256_final(c0_sha256_ctx *ctx, uint8_t output[C0_SHA256_BYTES]);
void c0_sha256(
    uint8_t output[C0_SHA256_BYTES],
    const uint8_t *input,
    size_t length
);

void c0_hmac_sha256_init(
    c0_hmac_sha256_ctx *ctx,
    const uint8_t *key,
    size_t key_length
);
void c0_hmac_sha256_update(
    c0_hmac_sha256_ctx *ctx,
    const uint8_t *input,
    size_t length
);
void c0_hmac_sha256_final(
    c0_hmac_sha256_ctx *ctx,
    uint8_t output[C0_HMAC_SHA256_BYTES]
);
void c0_hmac_sha256(
    uint8_t output[C0_HMAC_SHA256_BYTES],
    const uint8_t *key,
    size_t key_length,
    const uint8_t *input,
    size_t input_length
);

void c0_hkdf_sha256_extract(
    uint8_t prk[C0_SHA256_BYTES],
    const uint8_t *salt,
    size_t salt_length,
    const uint8_t *input_key_material,
    size_t input_key_material_length
);
int c0_hkdf_sha256_expand(
    uint8_t *output,
    size_t output_length,
    const uint8_t prk[C0_SHA256_BYTES],
    const uint8_t *info,
    size_t info_length
);

#ifdef __cplusplus
}
#endif

#endif

