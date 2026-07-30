#ifndef C0PQLINK_KECCAK_H
#define C0PQLINK_KECCAK_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t state[25];
    uint16_t rate;
    uint16_t position;
    uint8_t domain;
    uint8_t squeezing;
} c0_keccak_ctx;

void c0_sha3_256_init(c0_keccak_ctx *ctx);
void c0_sha3_512_init(c0_keccak_ctx *ctx);
void c0_shake128_init(c0_keccak_ctx *ctx);
void c0_shake256_init(c0_keccak_ctx *ctx);
void c0_keccak_absorb(c0_keccak_ctx *ctx, const uint8_t *input, size_t length);
void c0_keccak_finalize(c0_keccak_ctx *ctx);
void c0_keccak_squeeze(c0_keccak_ctx *ctx, uint8_t *output, size_t length);
void c0_sha3_256(uint8_t output[32], const uint8_t *input, size_t length);
void c0_sha3_512(uint8_t output[64], const uint8_t *input, size_t length);

#endif

