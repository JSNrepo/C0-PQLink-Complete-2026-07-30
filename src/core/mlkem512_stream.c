#include "c0pqlink/mlkem512_stream.h"

#include "core/keccak.h"
#include "c0pqlink/flash.h"

#include <string.h>

#define MLKEM_N 256u
#define MLKEM_Q 3329
#define MLKEM_K 2u
#define MLKEM_ETA1 3u
#define MLKEM_ETA2 2u
#define MLKEM_QINV 62209u

typedef struct {
    int16_t accumulator[MLKEM_N];
    int16_t temporary[MLKEM_N];
    c0_keccak_ctx keccak;
    uint8_t rho[32];
    uint8_t coins[32];
    uint8_t pack[8];
    uint16_t generated;
    uint16_t public_key_offset;
} mlkem_workspace_internal;

typedef char workspace_must_fit[
    (sizeof(mlkem_workspace_internal) <= C0_MLKEM512_WORKSPACE_BYTES) ? 1 : -1
];

static const int16_t zetas[128] C0PQLINK_FLASH = {
    -1044, -758, -359, -1517, 1493, 1422, 287, 202,
    -171, 622, 1577, 182, 962, -1202, -1474, 1468,
    573, -1325, 264, 383, -829, 1458, -1602, -130,
    -681, 1017, 732, 608, -1542, 411, -205, -1571,
    1223, 652, -552, 1015, -1293, 1491, -282, -1544,
    516, -8, -320, -666, -1618, -1162, 126, 1469,
    -853, -90, -271, 830, 107, -1421, -247, -951,
    -398, 961, -1508, -725, 448, -1065, 677, -1275,
    -1103, 430, 555, 843, -1251, 871, 1550, 105,
    422, 587, 177, -235, -291, -460, 1574, 1653,
    -246, 778, 1159, -147, -777, 1483, -602, 1119,
    -1590, 644, -872, 349, 418, 329, -156, -75,
    817, 1097, 603, 610, 1322, -1285, -1465, 384,
    -1215, -136, 1218, -1335, -874, 220, -1187, -1659,
    -1185, -1530, -1278, 794, -1510, -854, -870, 478,
    -108, -308, 996, 991, 958, -1460, 1522, 1628
};

static int16_t load_zeta(unsigned int index)
{
    return (int16_t)c0pqlink_flash_read_u16(
        (const uint16_t *)(const void *)&zetas[index]
    );
}

void c0pqlink_secure_zero(void *ptr, size_t len)
{
    volatile uint8_t *bytes = (volatile uint8_t *)ptr;
    while (len > 0u) {
        *bytes = 0u;
        ++bytes;
        --len;
    }
}

int c0pqlink_ct_equal(const uint8_t *a, const uint8_t *b, size_t len)
{
    uint8_t difference = 0u;
    size_t i;
    for (i = 0; i < len; ++i) {
        difference |= (uint8_t)(a[i] ^ b[i]);
    }
    return difference == 0u;
}

/*
 * AVR-GCC may implement signed 16x16 multiplication by branching on operand
 * signs. This helper requests only an unsigned 16x16 product, then applies the
 * two's-complement correction with masks. On an 8-bit AVR the intended linked
 * primitive is the fixed-instruction unsigned multiply helper.
 */
#if defined(C0PQLINK_CT_AUDIT) && defined(__GNUC__)
#define C0PQLINK_CT_AUDIT_ATTRIBUTE __attribute__((noinline, used))
#else
#define C0PQLINK_CT_AUDIT_ATTRIBUTE
#endif

static uint16_t multiply_u8_fixed(uint8_t a, uint8_t b)
{
#if defined(__AVR__) && defined(__AVR_HAVE_MUL__)
    uint16_t product;
    /*
     * ATmega328P has a fixed-latency unsigned 8x8 MUL instruction. Four
     * invocations form the unsigned 16x16 product below. Clearing r1 restores
     * AVR-GCC's zero-register ABI invariant.
     */
    __asm__ volatile(
        "mul %1, %2" "\n\t"
        "movw %0, r0" "\n\t"
        "clr __zero_reg__"
        : "=&r" (product)
        : "r" (a), "r" (b)
        : "r0", "r1"
    );
    return product;
#else
    return (uint16_t)((uint16_t)a * (uint16_t)b);
#endif
}

static uint32_t multiply_u16_fixed(uint16_t a, uint16_t b)
{
    const uint16_t p00 = multiply_u8_fixed(
        (uint8_t)a,
        (uint8_t)b
    );
    const uint16_t p01 = multiply_u8_fixed(
        (uint8_t)a,
        (uint8_t)(b >> 8u)
    );
    const uint16_t p10 = multiply_u8_fixed(
        (uint8_t)(a >> 8u),
        (uint8_t)b
    );
    const uint16_t p11 = multiply_u8_fixed(
        (uint8_t)(a >> 8u),
        (uint8_t)(b >> 8u)
    );
    return (uint32_t)p00
        + ((uint32_t)p01 << 8u)
        + ((uint32_t)p10 << 8u)
        + ((uint32_t)p11 << 16u);
}

static C0PQLINK_CT_AUDIT_ATTRIBUTE int32_t
multiply_s16_constant_schedule(int16_t a, int16_t b)
{
    const uint16_t ua = (uint16_t)a;
    const uint16_t ub = (uint16_t)b;
    const uint32_t a_negative = (uint32_t)(ua >> 15u);
    const uint32_t b_negative = (uint32_t)(ub >> 15u);
    uint32_t product = multiply_u16_fixed(ua, ub);
    const uint32_t a_correction = (
        UINT32_C(0) - a_negative
    ) & ((uint32_t)ub << 16u);
    const uint32_t b_correction = (
        UINT32_C(0) - b_negative
    ) & ((uint32_t)ua << 16u);
    product -= a_correction;
    product -= b_correction;
    return (int32_t)product;
}

static int16_t montgomery_reduce(int32_t value)
{
    const int16_t low = (int16_t)value;
    const int16_t factor = (int16_t)(
        (uint16_t)low * (uint16_t)MLKEM_QINV
    );
    const int32_t product = multiply_s16_constant_schedule(
        factor,
        (int16_t)MLKEM_Q
    );
    return (int16_t)((value - product) >> 16);
}

static int16_t barrett_reduce(int16_t value)
{
    const int16_t v = (int16_t)(
        ((INT32_C(1) << 26) + (MLKEM_Q / 2)) / MLKEM_Q
    );
    const int16_t quotient = (int16_t)(
        (multiply_s16_constant_schedule(v, value)
            + (INT32_C(1) << 25)) >> 26
    );
    return (int16_t)(
        value - (int16_t)multiply_s16_constant_schedule(
            quotient,
            (int16_t)MLKEM_Q
        )
    );
}

static int16_t fqmul(int16_t a, int16_t b)
{
    return montgomery_reduce(multiply_s16_constant_schedule(a, b));
}

static int16_t freeze_coefficient(int16_t value)
{
    int16_t reduced = barrett_reduce(value);
    const int16_t mask = (int16_t)(reduced >> 15);
    reduced = (int16_t)(reduced + (mask & MLKEM_Q));
    return reduced;
}

static void ntt(int16_t polynomial[MLKEM_N])
{
    unsigned int length;
    unsigned int start;
    unsigned int j;
    unsigned int k = 1u;

    for (length = 128u; length >= 2u; length >>= 1u) {
        for (start = 0u; start < MLKEM_N; start = j + length) {
            const int16_t zeta = load_zeta(k++);
            for (j = start; j < start + length; ++j) {
                const int16_t product = fqmul(zeta, polynomial[j + length]);
                polynomial[j + length] = (int16_t)(polynomial[j] - product);
                polynomial[j] = (int16_t)(polynomial[j] + product);
            }
        }
    }
}

static void inverse_ntt(int16_t polynomial[MLKEM_N])
{
    unsigned int length;
    unsigned int start;
    unsigned int j;
    unsigned int k = 127u;
    const int16_t scale = 1441;

    for (length = 2u; length <= 128u; length <<= 1u) {
        for (start = 0u; start < MLKEM_N; start = j + length) {
            const int16_t zeta = load_zeta(k--);
            for (j = start; j < start + length; ++j) {
                const int16_t first = polynomial[j];
                polynomial[j] = barrett_reduce(
                    (int16_t)(first + polynomial[j + length])
                );
                polynomial[j + length] = (int16_t)(
                    polynomial[j + length] - first
                );
                polynomial[j + length] = fqmul(
                    zeta,
                    polynomial[j + length]
                );
            }
        }
    }
    for (j = 0u; j < MLKEM_N; ++j) {
        polynomial[j] = fqmul(polynomial[j], scale);
    }
}

static void base_multiply_add(
    int16_t accumulator[2],
    int16_t a0,
    int16_t a1,
    int16_t b0,
    int16_t b1,
    int16_t zeta
)
{
    int16_t c0 = fqmul(a1, b1);
    c0 = fqmul(c0, zeta);
    c0 = (int16_t)(c0 + fqmul(a0, b0));
    accumulator[0] = (int16_t)(accumulator[0] + c0);
    accumulator[1] = (int16_t)(
        accumulator[1] + fqmul(a0, b1) + fqmul(a1, b0)
    );
}

static uint8_t popcount2(uint8_t value)
{
    return (uint8_t)((value & 1u) + ((value >> 1u) & 1u));
}

static void sample_cbd(
    int16_t polynomial[MLKEM_N],
    const uint8_t seed[32],
    uint8_t nonce,
    unsigned int eta,
    c0_keccak_ctx *ctx
)
{
    uint8_t input[33];
    unsigned int position = 0u;

    memcpy(input, seed, 32u);
    input[32] = nonce;
    c0_shake256_init(ctx);
    c0_keccak_absorb(ctx, input, sizeof(input));
    c0_keccak_finalize(ctx);

    if (eta == 2u) {
        while (position < MLKEM_N) {
            uint8_t byte;
            unsigned int half;
            c0_keccak_squeeze(ctx, &byte, 1u);
            for (half = 0u; half < 2u; ++half) {
                const uint8_t nibble = (uint8_t)(
                    byte >> (4u * half)
                );
                polynomial[position++] = (int16_t)(
                    (int16_t)popcount2(nibble)
                    - (int16_t)popcount2((uint8_t)(nibble >> 2u))
                );
            }
        }
    } else {
        while (position < MLKEM_N) {
            uint8_t bytes[3];
            uint32_t word;
            uint32_t sums;
            unsigned int i;
            c0_keccak_squeeze(ctx, bytes, sizeof(bytes));
            word = (uint32_t)bytes[0]
                | ((uint32_t)bytes[1] << 8u)
                | ((uint32_t)bytes[2] << 16u);
            sums = word & UINT32_C(0x00249249);
            sums += (word >> 1u) & UINT32_C(0x00249249);
            sums += (word >> 2u) & UINT32_C(0x00249249);
            for (i = 0u; i < 4u; ++i) {
                const int16_t a = (int16_t)((sums >> (6u * i)) & 7u);
                const int16_t b = (int16_t)(
                    (sums >> (6u * i + 3u)) & 7u
                );
                polynomial[position++] = (int16_t)(a - b);
            }
        }
    }
    c0pqlink_secure_zero(input, sizeof(input));
}

typedef struct {
    c0_keccak_ctx *ctx;
    uint8_t bytes[3];
    unsigned int available;
    unsigned int next;
} sample_ntt_reader;

static void sample_ntt_reader_init(
    sample_ntt_reader *reader,
    c0_keccak_ctx *ctx,
    const uint8_t rho[32],
    uint8_t x,
    uint8_t y
)
{
    uint8_t suffix[2];
    c0_shake128_init(ctx);
    c0_keccak_absorb(ctx, rho, 32u);
    suffix[0] = x;
    suffix[1] = y;
    c0_keccak_absorb(ctx, suffix, sizeof(suffix));
    c0_keccak_finalize(ctx);
    reader->ctx = ctx;
    reader->available = 0u;
    reader->next = 0u;
}

static uint16_t sample_ntt_next(sample_ntt_reader *reader)
{
    for (;;) {
        uint16_t value;
        if (reader->available == 0u) {
            c0_keccak_squeeze(reader->ctx, reader->bytes, 3u);
            reader->available = 2u;
            reader->next = 0u;
        }
        if (reader->next == 0u) {
            value = (uint16_t)(
                (uint16_t)reader->bytes[0]
                | ((uint16_t)(reader->bytes[1] & 0x0fu) << 8u)
            );
            reader->next = 1u;
        } else {
            value = (uint16_t)(
                ((uint16_t)reader->bytes[1] >> 4u)
                | ((uint16_t)reader->bytes[2] << 4u)
            );
            reader->available = 0u;
        }
        if (value < MLKEM_Q) {
            return value;
        }
    }
}

static void matrix_product_add(
    int16_t accumulator[MLKEM_N],
    const int16_t vector_polynomial[MLKEM_N],
    const uint8_t rho[32],
    uint8_t x,
    uint8_t y,
    c0_keccak_ctx *ctx
)
{
    sample_ntt_reader reader;
    unsigned int block;

    sample_ntt_reader_init(&reader, ctx, rho, x, y);
    for (block = 0u; block < 64u; ++block) {
        const int16_t zeta = zetas[64u + block];
        const int16_t a0 = (int16_t)sample_ntt_next(&reader);
        const int16_t a1 = (int16_t)sample_ntt_next(&reader);
        const int16_t a2 = (int16_t)sample_ntt_next(&reader);
        const int16_t a3 = (int16_t)sample_ntt_next(&reader);
        base_multiply_add(
            &accumulator[4u * block],
            a0,
            a1,
            vector_polynomial[4u * block],
            vector_polynomial[4u * block + 1u],
            zeta
        );
        base_multiply_add(
            &accumulator[4u * block + 2u],
            a2,
            a3,
            vector_polynomial[4u * block + 2u],
            vector_polynomial[4u * block + 3u],
            (int16_t)-zeta
        );
    }
}

static int validate_and_hash_public_key(
    c0_mlkem512_pk_read_fn read_public_key,
    void *context,
    uint8_t hash[32],
    uint8_t rho[32],
    c0_keccak_ctx *ctx
)
{
    uint16_t offset;
    uint8_t triple[3];

    c0_sha3_256_init(ctx);
    for (offset = 0u; offset < C0_MLKEM512_PUBLIC_KEY_BYTES; ++offset) {
        const uint8_t byte = read_public_key(context, offset);
        c0_keccak_absorb(ctx, &byte, 1u);
    }
    c0_keccak_squeeze(ctx, hash, 32u);

    for (offset = 0u; offset < 768u; offset += 3u) {
        uint16_t first;
        uint16_t second;
        triple[0] = read_public_key(context, offset);
        triple[1] = read_public_key(context, (uint16_t)(offset + 1u));
        triple[2] = read_public_key(context, (uint16_t)(offset + 2u));
        first = (uint16_t)(
            (uint16_t)triple[0]
            | ((uint16_t)(triple[1] & 0x0fu) << 8u)
        );
        second = (uint16_t)(
            ((uint16_t)triple[1] >> 4u)
            | ((uint16_t)triple[2] << 4u)
        );
        if (first >= MLKEM_Q || second >= MLKEM_Q) {
            return C0PQLINK_ERR_KEY_ENCODING;
        }
    }
    for (offset = 0u; offset < 32u; ++offset) {
        rho[offset] = read_public_key(
            context,
            (uint16_t)(768u + offset)
        );
    }
    return C0PQLINK_OK;
}

static uint16_t compress10(int16_t coefficient)
{
    const uint32_t value = (uint16_t)freeze_coefficient(coefficient);
    const uint64_t scaled = (
        ((uint64_t)value << 10u) + UINT64_C(1665)
    ) * UINT64_C(1290167);
    return (uint16_t)((scaled >> 32u) & UINT64_C(0x03ff));
}

static uint8_t compress4(int16_t coefficient)
{
    const uint32_t value = (uint16_t)freeze_coefficient(coefficient);
    uint32_t scaled = (value << 4u) + UINT32_C(1665);
    scaled *= UINT32_C(80635);
    return (uint8_t)((scaled >> 28u) & UINT32_C(0x0f));
}

static int write_compressed_u(
    const int16_t polynomial[MLKEM_N],
    c0_mlkem512_ct_write_fn write_ciphertext,
    void *context,
    uint8_t pack[8]
)
{
    unsigned int i;
    for (i = 0u; i < MLKEM_N; i += 4u) {
        const uint16_t t0 = compress10(polynomial[i]);
        const uint16_t t1 = compress10(polynomial[i + 1u]);
        const uint16_t t2 = compress10(polynomial[i + 2u]);
        const uint16_t t3 = compress10(polynomial[i + 3u]);
        pack[0] = (uint8_t)t0;
        pack[1] = (uint8_t)((t0 >> 8u) | (t1 << 2u));
        pack[2] = (uint8_t)((t1 >> 6u) | (t2 << 4u));
        pack[3] = (uint8_t)((t2 >> 4u) | (t3 << 6u));
        pack[4] = (uint8_t)(t3 >> 2u);
        if (write_ciphertext(context, pack, 5u) != 0) {
            return C0PQLINK_ERR_IO;
        }
    }
    return C0PQLINK_OK;
}

static int write_compressed_v(
    const int16_t polynomial[MLKEM_N],
    const uint8_t message[32],
    c0_mlkem512_ct_write_fn write_ciphertext,
    void *context,
    uint8_t pack[8]
)
{
    unsigned int i;
    for (i = 0u; i < MLKEM_N; i += 2u) {
        const uint16_t message_bit0 = (uint16_t)(
            ((uint16_t)message[i >> 3u] >> (i & 7u)) & UINT16_C(1)
        );
        const uint16_t message_bit1 = (uint16_t)(
            ((uint16_t)message[(i + 1u) >> 3u]
                >> ((i + 1u) & 7u)) & UINT16_C(1)
        );
        const int16_t m0 = (int16_t)(
            (uint16_t)(UINT16_C(0) - message_bit0) & UINT16_C(1665)
        );
        const int16_t m1 = (int16_t)(
            (uint16_t)(UINT16_C(0) - message_bit1) & UINT16_C(1665)
        );
        const uint8_t low = compress4((int16_t)(polynomial[i] + m0));
        const uint8_t high = compress4(
            (int16_t)(polynomial[i + 1u] + m1)
        );
        pack[0] = (uint8_t)(low | (uint8_t)(high << 4u));
        if (write_ciphertext(context, pack, 1u) != 0) {
            return C0PQLINK_ERR_IO;
        }
    }
    return C0PQLINK_OK;
}

static void derive_k_and_coins(
    mlkem_workspace_internal *state,
    const uint8_t randomness[32],
    const uint8_t public_key_hash[32],
    uint8_t shared_secret[32]
)
{
    c0_sha3_512_init(&state->keccak);
    c0_keccak_absorb(&state->keccak, randomness, 32u);
    c0_keccak_absorb(&state->keccak, public_key_hash, 32u);
    c0_keccak_squeeze(&state->keccak, shared_secret, 32u);
    c0_keccak_squeeze(&state->keccak, state->coins, 32u);
}

int c0_mlkem512_encapsulate_derand(
    c0_mlkem512_pk_read_fn read_public_key,
    void *public_key_context,
    const uint8_t randomness[32],
    c0_mlkem512_ct_write_fn write_ciphertext,
    void *ciphertext_context,
    uint8_t shared_secret[32],
    c0_mlkem512_workspace *workspace
)
{
    mlkem_workspace_internal *state;
    uint8_t public_key_hash[32];
    unsigned int row;
    unsigned int column;
    int result;

    if (read_public_key == NULL || randomness == NULL
        || write_ciphertext == NULL || shared_secret == NULL
        || workspace == NULL) {
        return C0PQLINK_ERR_ARGUMENT;
    }

    state = (mlkem_workspace_internal *)(void *)workspace->bytes;
    memset(state, 0, sizeof(*state));

    result = validate_and_hash_public_key(
        read_public_key,
        public_key_context,
        public_key_hash,
        state->rho,
        &state->keccak
    );
    if (result != C0PQLINK_OK) {
        c0pqlink_secure_zero(state, sizeof(*state));
        c0pqlink_secure_zero(public_key_hash, sizeof(public_key_hash));
        return result;
    }

    derive_k_and_coins(
        state,
        randomness,
        public_key_hash,
        shared_secret
    );

    for (row = 0u; row < MLKEM_K; ++row) {
        memset(state->accumulator, 0, sizeof(state->accumulator));
        for (column = 0u; column < MLKEM_K; ++column) {
            sample_cbd(
                state->temporary,
                state->coins,
                (uint8_t)column,
                MLKEM_ETA1,
                &state->keccak
            );
            ntt(state->temporary);
            matrix_product_add(
                state->accumulator,
                state->temporary,
                state->rho,
                (uint8_t)row,
                (uint8_t)column,
                &state->keccak
            );
        }
        for (column = 0u; column < MLKEM_N; ++column) {
            state->accumulator[column] = barrett_reduce(
                state->accumulator[column]
            );
        }
        inverse_ntt(state->accumulator);
        sample_cbd(
            state->temporary,
            state->coins,
            (uint8_t)(MLKEM_K + row),
            MLKEM_ETA2,
            &state->keccak
        );
        for (column = 0u; column < MLKEM_N; ++column) {
            state->accumulator[column] = (int16_t)(
                state->accumulator[column] + state->temporary[column]
            );
        }
        result = write_compressed_u(
            state->accumulator,
            write_ciphertext,
            ciphertext_context,
            state->pack
        );
        if (result != C0PQLINK_OK) {
            c0pqlink_secure_zero(shared_secret, 32u);
            c0pqlink_secure_zero(state, sizeof(*state));
            c0pqlink_secure_zero(public_key_hash, sizeof(public_key_hash));
            return result;
        }
    }

    /*
     * Recompute the public-key dot product coefficient block by coefficient
     * block. `temporary` holds rHat; public coefficients are decoded into a
     * four-coefficient local buffer so the persistent accumulator remains v.
     */
    memset(state->accumulator, 0, sizeof(state->accumulator));
    for (column = 0u; column < MLKEM_K; ++column) {
        unsigned int block;
        sample_cbd(
            state->temporary,
            state->coins,
            (uint8_t)column,
            MLKEM_ETA1,
            &state->keccak
        );
        ntt(state->temporary);
        for (block = 0u; block < 64u; ++block) {
            const uint16_t base = (uint16_t)(
                column * 384u + block * 6u
            );
            const uint8_t b0 = read_public_key(public_key_context, base);
            const uint8_t b1 = read_public_key(
                public_key_context,
                (uint16_t)(base + 1u)
            );
            const uint8_t b2 = read_public_key(
                public_key_context,
                (uint16_t)(base + 2u)
            );
            const uint8_t b3 = read_public_key(
                public_key_context,
                (uint16_t)(base + 3u)
            );
            const uint8_t b4 = read_public_key(
                public_key_context,
                (uint16_t)(base + 4u)
            );
            const uint8_t b5 = read_public_key(
                public_key_context,
                (uint16_t)(base + 5u)
            );
            const int16_t a0 = (int16_t)(
                (uint16_t)b0 | ((uint16_t)(b1 & 0x0fu) << 8u)
            );
            const int16_t a1 = (int16_t)(
                ((uint16_t)b1 >> 4u) | ((uint16_t)b2 << 4u)
            );
            const int16_t a2 = (int16_t)(
                (uint16_t)b3 | ((uint16_t)(b4 & 0x0fu) << 8u)
            );
            const int16_t a3 = (int16_t)(
                ((uint16_t)b4 >> 4u) | ((uint16_t)b5 << 4u)
            );
            const int16_t zeta = load_zeta(64u + block);
            base_multiply_add(
                &state->accumulator[4u * block],
                a0,
                a1,
                state->temporary[4u * block],
                state->temporary[4u * block + 1u],
                zeta
            );
            base_multiply_add(
                &state->accumulator[4u * block + 2u],
                a2,
                a3,
                state->temporary[4u * block + 2u],
                state->temporary[4u * block + 3u],
                (int16_t)-zeta
            );
        }
    }
    for (column = 0u; column < MLKEM_N; ++column) {
        state->accumulator[column] = barrett_reduce(
            state->accumulator[column]
        );
    }
    inverse_ntt(state->accumulator);
    sample_cbd(
        state->temporary,
        state->coins,
        (uint8_t)(2u * MLKEM_K),
        MLKEM_ETA2,
        &state->keccak
    );
    for (column = 0u; column < MLKEM_N; ++column) {
        state->accumulator[column] = (int16_t)(
            state->accumulator[column] + state->temporary[column]
        );
    }
    result = write_compressed_v(
        state->accumulator,
        randomness,
        write_ciphertext,
        ciphertext_context,
        state->pack
    );
    if (result != C0PQLINK_OK) {
        c0pqlink_secure_zero(shared_secret, 32u);
    }

    c0pqlink_secure_zero(state, sizeof(*state));
    c0pqlink_secure_zero(public_key_hash, sizeof(public_key_hash));
    return result;
}

int c0_mlkem512_encapsulate(
    c0_mlkem512_pk_read_fn read_public_key,
    void *public_key_context,
    c0pqlink_rng_fn random_bytes,
    void *rng_context,
    c0_mlkem512_ct_write_fn write_ciphertext,
    void *ciphertext_context,
    uint8_t shared_secret[32],
    c0_mlkem512_workspace *workspace
)
{
    uint8_t randomness[32];
    int result;
    if (random_bytes == NULL) {
        return C0PQLINK_ERR_ARGUMENT;
    }
    if (random_bytes(rng_context, randomness, sizeof(randomness)) != 0) {
        c0pqlink_secure_zero(randomness, sizeof(randomness));
        return C0PQLINK_ERR_RNG;
    }
    result = c0_mlkem512_encapsulate_derand(
        read_public_key,
        public_key_context,
        randomness,
        write_ciphertext,
        ciphertext_context,
        shared_secret,
        workspace
    );
    c0pqlink_secure_zero(randomness, sizeof(randomness));
    return result;
}

size_t c0_mlkem512_workspace_bytes(void)
{
    return sizeof(mlkem_workspace_internal);
}
