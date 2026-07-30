#include "core/keccak.h"
#include "c0pqlink/flash.h"

#include <string.h>

static uint64_t rotate_left64(uint64_t value, unsigned int count)
{
    return (value << count) | (value >> (64u - count));
}

static const uint64_t keccak_round_constants[24] C0PQLINK_FLASH = {
    UINT64_C(0x0000000000000001), UINT64_C(0x0000000000008082),
    UINT64_C(0x800000000000808a), UINT64_C(0x8000000080008000),
    UINT64_C(0x000000000000808b), UINT64_C(0x0000000080000001),
    UINT64_C(0x8000000080008081), UINT64_C(0x8000000000008009),
    UINT64_C(0x000000000000008a), UINT64_C(0x0000000000000088),
    UINT64_C(0x0000000080008009), UINT64_C(0x000000008000000a),
    UINT64_C(0x000000008000808b), UINT64_C(0x800000000000008b),
    UINT64_C(0x8000000000008089), UINT64_C(0x8000000000008003),
    UINT64_C(0x8000000000008002), UINT64_C(0x8000000000000080),
    UINT64_C(0x000000000000800a), UINT64_C(0x800000008000000a),
    UINT64_C(0x8000000080008081), UINT64_C(0x8000000000008080),
    UINT64_C(0x0000000080000001), UINT64_C(0x8000000080008008)
};

static const uint8_t keccak_rotation[24] C0PQLINK_FLASH = {
    1u, 3u, 6u, 10u, 15u, 21u, 28u, 36u,
    45u, 55u, 2u, 14u, 27u, 41u, 56u, 8u,
    25u, 43u, 62u, 18u, 39u, 61u, 20u, 44u
};

static const uint8_t keccak_pi_lane[24] C0PQLINK_FLASH = {
    10u, 7u, 11u, 17u, 18u, 3u, 5u, 16u,
    8u, 21u, 24u, 4u, 15u, 23u, 19u, 13u,
    12u, 2u, 20u, 14u, 22u, 9u, 6u, 1u
};

static void keccak_f1600(uint64_t state[25])
{
    uint64_t bc[5];
    uint64_t current;
    uint64_t next;
    unsigned int round;
    unsigned int i;
    unsigned int j;

    for (round = 0; round < 24u; ++round) {
        for (i = 0; i < 5u; ++i) {
            bc[i] = state[i] ^ state[i + 5u] ^ state[i + 10u]
                ^ state[i + 15u] ^ state[i + 20u];
        }
        for (i = 0; i < 5u; ++i) {
            const uint64_t theta = bc[(i + 4u) % 5u]
                ^ rotate_left64(bc[(i + 1u) % 5u], 1u);
            for (j = 0; j < 25u; j += 5u) {
                state[j + i] ^= theta;
            }
        }

        current = state[1];
        for (i = 0; i < 24u; ++i) {
            const unsigned int lane =
                c0pqlink_flash_read_u8(&keccak_pi_lane[i]);
            next = state[lane];
            state[lane] = rotate_left64(
                current,
                c0pqlink_flash_read_u8(&keccak_rotation[i])
            );
            current = next;
        }

        for (j = 0; j < 25u; j += 5u) {
            for (i = 0; i < 5u; ++i) {
                bc[i] = state[j + i];
            }
            for (i = 0; i < 5u; ++i) {
                state[j + i] = bc[i] ^ ((~bc[(i + 1u) % 5u])
                    & bc[(i + 2u) % 5u]);
            }
        }
        state[0] ^= c0pqlink_flash_read_u64(
            &keccak_round_constants[round]
        );
    }
}

static void keccak_init(
    c0_keccak_ctx *ctx,
    uint16_t rate,
    uint8_t domain
)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->rate = rate;
    ctx->domain = domain;
}

void c0_sha3_256_init(c0_keccak_ctx *ctx)
{
    keccak_init(ctx, 136u, 0x06u);
}

void c0_sha3_512_init(c0_keccak_ctx *ctx)
{
    keccak_init(ctx, 72u, 0x06u);
}

void c0_shake128_init(c0_keccak_ctx *ctx)
{
    keccak_init(ctx, 168u, 0x1fu);
}

void c0_shake256_init(c0_keccak_ctx *ctx)
{
    keccak_init(ctx, 136u, 0x1fu);
}

void c0_keccak_absorb(
    c0_keccak_ctx *ctx,
    const uint8_t *input,
    size_t length
)
{
    size_t i;

    if (ctx->squeezing != 0u) {
        return;
    }
    for (i = 0; i < length; ++i) {
        const unsigned int lane = (unsigned int)(ctx->position >> 3u);
        const unsigned int shift = 8u * (unsigned int)(ctx->position & 7u);
        ctx->state[lane] ^= ((uint64_t)input[i]) << shift;
        ++ctx->position;
        if (ctx->position == ctx->rate) {
            keccak_f1600(ctx->state);
            ctx->position = 0u;
        }
    }
}

void c0_keccak_finalize(c0_keccak_ctx *ctx)
{
    unsigned int lane;
    unsigned int shift;
    if (ctx->squeezing != 0u) {
        return;
    }
    lane = (unsigned int)(ctx->position >> 3u);
    shift = 8u * (unsigned int)(ctx->position & 7u);
    ctx->state[lane] ^= ((uint64_t)ctx->domain) << shift;
    lane = (unsigned int)((ctx->rate - 1u) >> 3u);
    shift = 8u * (unsigned int)((ctx->rate - 1u) & 7u);
    ctx->state[lane] ^= UINT64_C(0x80) << shift;
    keccak_f1600(ctx->state);
    ctx->position = 0u;
    ctx->squeezing = 1u;
}

void c0_keccak_squeeze(
    c0_keccak_ctx *ctx,
    uint8_t *output,
    size_t length
)
{
    size_t i;

    if (ctx->squeezing == 0u) {
        c0_keccak_finalize(ctx);
    }
    for (i = 0; i < length; ++i) {
        const unsigned int lane = (unsigned int)(ctx->position >> 3u);
        const unsigned int shift = 8u * (unsigned int)(ctx->position & 7u);
        output[i] = (uint8_t)(ctx->state[lane] >> shift);
        ++ctx->position;
        if (ctx->position == ctx->rate) {
            keccak_f1600(ctx->state);
            ctx->position = 0u;
        }
    }
}

void c0_sha3_256(
    uint8_t output[32],
    const uint8_t *input,
    size_t length
)
{
    c0_keccak_ctx ctx;
    c0_sha3_256_init(&ctx);
    c0_keccak_absorb(&ctx, input, length);
    c0_keccak_squeeze(&ctx, output, 32u);
    memset(&ctx, 0, sizeof(ctx));
}

void c0_sha3_512(
    uint8_t output[64],
    const uint8_t *input,
    size_t length
)
{
    c0_keccak_ctx ctx;
    c0_sha3_512_init(&ctx);
    c0_keccak_absorb(&ctx, input, length);
    c0_keccak_squeeze(&ctx, output, 64u);
    memset(&ctx, 0, sizeof(ctx));
}
