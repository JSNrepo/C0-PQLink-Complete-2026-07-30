#include "c0pqlink/sha256.h"
#include "c0pqlink/flash.h"

#include <string.h>

static const uint32_t round_constants[64] C0PQLINK_FLASH = {
    UINT32_C(0x428a2f98), UINT32_C(0x71374491),
    UINT32_C(0xb5c0fbcf), UINT32_C(0xe9b5dba5),
    UINT32_C(0x3956c25b), UINT32_C(0x59f111f1),
    UINT32_C(0x923f82a4), UINT32_C(0xab1c5ed5),
    UINT32_C(0xd807aa98), UINT32_C(0x12835b01),
    UINT32_C(0x243185be), UINT32_C(0x550c7dc3),
    UINT32_C(0x72be5d74), UINT32_C(0x80deb1fe),
    UINT32_C(0x9bdc06a7), UINT32_C(0xc19bf174),
    UINT32_C(0xe49b69c1), UINT32_C(0xefbe4786),
    UINT32_C(0x0fc19dc6), UINT32_C(0x240ca1cc),
    UINT32_C(0x2de92c6f), UINT32_C(0x4a7484aa),
    UINT32_C(0x5cb0a9dc), UINT32_C(0x76f988da),
    UINT32_C(0x983e5152), UINT32_C(0xa831c66d),
    UINT32_C(0xb00327c8), UINT32_C(0xbf597fc7),
    UINT32_C(0xc6e00bf3), UINT32_C(0xd5a79147),
    UINT32_C(0x06ca6351), UINT32_C(0x14292967),
    UINT32_C(0x27b70a85), UINT32_C(0x2e1b2138),
    UINT32_C(0x4d2c6dfc), UINT32_C(0x53380d13),
    UINT32_C(0x650a7354), UINT32_C(0x766a0abb),
    UINT32_C(0x81c2c92e), UINT32_C(0x92722c85),
    UINT32_C(0xa2bfe8a1), UINT32_C(0xa81a664b),
    UINT32_C(0xc24b8b70), UINT32_C(0xc76c51a3),
    UINT32_C(0xd192e819), UINT32_C(0xd6990624),
    UINT32_C(0xf40e3585), UINT32_C(0x106aa070),
    UINT32_C(0x19a4c116), UINT32_C(0x1e376c08),
    UINT32_C(0x2748774c), UINT32_C(0x34b0bcb5),
    UINT32_C(0x391c0cb3), UINT32_C(0x4ed8aa4a),
    UINT32_C(0x5b9cca4f), UINT32_C(0x682e6ff3),
    UINT32_C(0x748f82ee), UINT32_C(0x78a5636f),
    UINT32_C(0x84c87814), UINT32_C(0x8cc70208),
    UINT32_C(0x90befffa), UINT32_C(0xa4506ceb),
    UINT32_C(0xbef9a3f7), UINT32_C(0xc67178f2)
};

static uint32_t rotate_right32(uint32_t value, unsigned int count)
{
    return (value >> count) | (value << (32u - count));
}

static uint32_t load32_be(const uint8_t bytes[4])
{
    return ((uint32_t)bytes[0] << 24u)
        | ((uint32_t)bytes[1] << 16u)
        | ((uint32_t)bytes[2] << 8u)
        | (uint32_t)bytes[3];
}

static void store32_be(uint8_t bytes[4], uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24u);
    bytes[1] = (uint8_t)(value >> 16u);
    bytes[2] = (uint8_t)(value >> 8u);
    bytes[3] = (uint8_t)value;
}

static void sha256_transform(c0_sha256_ctx *ctx, const uint8_t block[64])
{
    uint32_t schedule[16];
    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];
    uint32_t e = ctx->state[4];
    uint32_t f = ctx->state[5];
    uint32_t g = ctx->state[6];
    uint32_t h = ctx->state[7];
    unsigned int round;

    for (round = 0u; round < 16u; ++round) {
        schedule[round] = load32_be(block + 4u * round);
    }
    for (round = 0u; round < 64u; ++round) {
        uint32_t word;
        uint32_t sigma0;
        uint32_t sigma1;
        uint32_t choice;
        uint32_t majority;
        uint32_t sum0;
        uint32_t sum1;
        uint32_t temp1;
        uint32_t temp2;

        if (round >= 16u) {
            const uint32_t x = schedule[(round - 15u) & 15u];
            const uint32_t y = schedule[(round - 2u) & 15u];
            sigma0 = rotate_right32(x, 7u) ^ rotate_right32(x, 18u)
                ^ (x >> 3u);
            sigma1 = rotate_right32(y, 17u) ^ rotate_right32(y, 19u)
                ^ (y >> 10u);
            schedule[round & 15u] += sigma0
                + schedule[(round - 7u) & 15u] + sigma1;
        }
        word = schedule[round & 15u];
        sum1 = rotate_right32(e, 6u) ^ rotate_right32(e, 11u)
            ^ rotate_right32(e, 25u);
        choice = (e & f) ^ ((~e) & g);
        temp1 = h + sum1 + choice
            + c0pqlink_flash_read_u32(&round_constants[round]) + word;
        sum0 = rotate_right32(a, 2u) ^ rotate_right32(a, 13u)
            ^ rotate_right32(a, 22u);
        majority = (a & b) ^ (a & c) ^ (b & c);
        temp2 = sum0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
    c0pqlink_secure_zero(schedule, sizeof(schedule));
}

void c0_sha256_init(c0_sha256_ctx *ctx)
{
    ctx->state[0] = UINT32_C(0x6a09e667);
    ctx->state[1] = UINT32_C(0xbb67ae85);
    ctx->state[2] = UINT32_C(0x3c6ef372);
    ctx->state[3] = UINT32_C(0xa54ff53a);
    ctx->state[4] = UINT32_C(0x510e527f);
    ctx->state[5] = UINT32_C(0x9b05688c);
    ctx->state[6] = UINT32_C(0x1f83d9ab);
    ctx->state[7] = UINT32_C(0x5be0cd19);
    ctx->total_bytes = 0u;
    ctx->buffered = 0u;
    memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

void c0_sha256_update(
    c0_sha256_ctx *ctx,
    const uint8_t *input,
    size_t length
)
{
    size_t used = 0u;
    if (length == 0u) {
        return;
    }
    ctx->total_bytes += length;
    if (ctx->buffered != 0u) {
        const size_t available = 64u - ctx->buffered;
        const size_t take = length < available ? length : available;
        memcpy(ctx->buffer + ctx->buffered, input, take);
        ctx->buffered = (uint8_t)(ctx->buffered + take);
        used += take;
        if (ctx->buffered == 64u) {
            sha256_transform(ctx, ctx->buffer);
            ctx->buffered = 0u;
        }
    }
    while (length - used >= 64u) {
        sha256_transform(ctx, input + used);
        used += 64u;
    }
    if (used < length) {
        const size_t remainder = length - used;
        memcpy(ctx->buffer, input + used, remainder);
        ctx->buffered = (uint8_t)remainder;
    }
}

void c0_sha256_final(c0_sha256_ctx *ctx, uint8_t output[32])
{
    uint64_t total_bits = ctx->total_bytes * UINT64_C(8);
    unsigned int i;
    ctx->buffer[ctx->buffered++] = 0x80u;
    if (ctx->buffered > 56u) {
        memset(ctx->buffer + ctx->buffered, 0, 64u - ctx->buffered);
        sha256_transform(ctx, ctx->buffer);
        ctx->buffered = 0u;
    }
    memset(ctx->buffer + ctx->buffered, 0, 56u - ctx->buffered);
    for (i = 0u; i < 8u; ++i) {
        ctx->buffer[63u - i] = (uint8_t)total_bits;
        total_bits >>= 8u;
    }
    sha256_transform(ctx, ctx->buffer);
    for (i = 0u; i < 8u; ++i) {
        store32_be(output + 4u * i, ctx->state[i]);
    }
    c0pqlink_secure_zero(ctx, sizeof(*ctx));
}

void c0_sha256(uint8_t output[32], const uint8_t *input, size_t length)
{
    c0_sha256_ctx ctx;
    c0_sha256_init(&ctx);
    c0_sha256_update(&ctx, input, length);
    c0_sha256_final(&ctx, output);
}

void c0_hmac_sha256_init(
    c0_hmac_sha256_ctx *ctx,
    const uint8_t *key,
    size_t key_length
)
{
    uint8_t key_block[64];
    uint8_t digest[32];
    unsigned int i;
    memset(key_block, 0, sizeof(key_block));
    if (key_length > sizeof(key_block)) {
        c0_sha256(digest, key, key_length);
        memcpy(key_block, digest, sizeof(digest));
        c0pqlink_secure_zero(digest, sizeof(digest));
    } else if (key_length != 0u) {
        memcpy(key_block, key, key_length);
    }
    for (i = 0u; i < 64u; ++i) {
        ctx->outer_pad[i] = (uint8_t)(key_block[i] ^ 0x5cu);
        key_block[i] ^= 0x36u;
    }
    c0_sha256_init(&ctx->inner);
    c0_sha256_update(&ctx->inner, key_block, sizeof(key_block));
    c0pqlink_secure_zero(key_block, sizeof(key_block));
}

void c0_hmac_sha256_update(
    c0_hmac_sha256_ctx *ctx,
    const uint8_t *input,
    size_t length
)
{
    c0_sha256_update(&ctx->inner, input, length);
}

void c0_hmac_sha256_final(c0_hmac_sha256_ctx *ctx, uint8_t output[32])
{
    uint8_t inner_digest[32];
    c0_sha256_ctx outer;
    c0_sha256_final(&ctx->inner, inner_digest);
    c0_sha256_init(&outer);
    c0_sha256_update(&outer, ctx->outer_pad, sizeof(ctx->outer_pad));
    c0_sha256_update(&outer, inner_digest, sizeof(inner_digest));
    c0_sha256_final(&outer, output);
    c0pqlink_secure_zero(inner_digest, sizeof(inner_digest));
    c0pqlink_secure_zero(ctx, sizeof(*ctx));
}

void c0_hmac_sha256(
    uint8_t output[32],
    const uint8_t *key,
    size_t key_length,
    const uint8_t *input,
    size_t input_length
)
{
    c0_hmac_sha256_ctx ctx;
    c0_hmac_sha256_init(&ctx, key, key_length);
    c0_hmac_sha256_update(&ctx, input, input_length);
    c0_hmac_sha256_final(&ctx, output);
}

void c0_hkdf_sha256_extract(
    uint8_t prk[32],
    const uint8_t *salt,
    size_t salt_length,
    const uint8_t *input_key_material,
    size_t input_key_material_length
)
{
    if (salt == NULL || salt_length == 0u) {
        /*
         * HMAC pads both an empty key and an all-zero 32-byte key to the
         * same 64-byte zero block.
         */
        salt = NULL;
        salt_length = 0u;
    }
    c0_hmac_sha256(
        prk,
        salt,
        salt_length,
        input_key_material,
        input_key_material_length
    );
}

int c0_hkdf_sha256_expand(
    uint8_t *output,
    size_t output_length,
    const uint8_t prk[32],
    const uint8_t *info,
    size_t info_length
)
{
    uint8_t previous[32];
    uint8_t counter = 1u;
    size_t produced = 0u;
    size_t previous_length = 0u;
    if (output_length > 255u * 32u) {
        return C0PQLINK_ERR_CAPACITY;
    }
    while (produced < output_length) {
        c0_hmac_sha256_ctx hmac;
        const size_t take = output_length - produced < 32u
            ? output_length - produced : 32u;
        c0_hmac_sha256_init(&hmac, prk, 32u);
        c0_hmac_sha256_update(&hmac, previous, previous_length);
        c0_hmac_sha256_update(&hmac, info, info_length);
        c0_hmac_sha256_update(&hmac, &counter, 1u);
        c0_hmac_sha256_final(&hmac, previous);
        memcpy(output + produced, previous, take);
        produced += take;
        previous_length = 32u;
        ++counter;
    }
    c0pqlink_secure_zero(previous, sizeof(previous));
    return C0PQLINK_OK;
}
