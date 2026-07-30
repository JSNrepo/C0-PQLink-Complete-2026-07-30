#include "ascon_aead128.h"

#include <string.h>

typedef struct {
    uint64_t x[5];
} ascon_state;

static uint64_t rotate_right64(uint64_t value, unsigned int count)
{
    return (value >> count) | (value << (64u - count));
}

static uint64_t load_little_endian(const uint8_t *bytes, size_t length)
{
    uint64_t value = 0u;
    size_t i;
    for (i = 0u; i < length; ++i) {
        value |= ((uint64_t)bytes[i]) << (8u * i);
    }
    return value;
}

static void store_little_endian(uint8_t *bytes, uint64_t value, size_t length)
{
    size_t i;
    for (i = 0u; i < length; ++i) {
        bytes[i] = (uint8_t)(value >> (8u * i));
    }
}

static uint64_t clear_low_bytes(uint64_t value, size_t length)
{
    size_t i;
    for (i = 0u; i < length; ++i) {
        value &= ~(UINT64_C(0xff) << (8u * i));
    }
    return value;
}

static void ascon_round(ascon_state *state, uint8_t constant)
{
    uint64_t t0;
    uint64_t t1;
    uint64_t t2;
    uint64_t t3;
    uint64_t t4;
    state->x[2] ^= constant;
    state->x[0] ^= state->x[4];
    state->x[4] ^= state->x[3];
    state->x[2] ^= state->x[1];
    t0 = state->x[0] ^ (~state->x[1] & state->x[2]);
    t1 = state->x[1] ^ (~state->x[2] & state->x[3]);
    t2 = state->x[2] ^ (~state->x[3] & state->x[4]);
    t3 = state->x[3] ^ (~state->x[4] & state->x[0]);
    t4 = state->x[4] ^ (~state->x[0] & state->x[1]);
    t1 ^= t0;
    t0 ^= t4;
    t3 ^= t2;
    t2 = ~t2;
    state->x[0] = t0 ^ rotate_right64(t0, 19u)
        ^ rotate_right64(t0, 28u);
    state->x[1] = t1 ^ rotate_right64(t1, 61u)
        ^ rotate_right64(t1, 39u);
    state->x[2] = t2 ^ rotate_right64(t2, 1u)
        ^ rotate_right64(t2, 6u);
    state->x[3] = t3 ^ rotate_right64(t3, 10u)
        ^ rotate_right64(t3, 17u);
    state->x[4] = t4 ^ rotate_right64(t4, 7u)
        ^ rotate_right64(t4, 41u);
}

static void ascon_permute12(ascon_state *state)
{
    unsigned int i;
    for (i = 0u; i < 12u; ++i) {
        ascon_round(
            state,
            (uint8_t)(((15u - i) << 4u) | i)
        );
    }
}

static void ascon_permute8(ascon_state *state)
{
    unsigned int i;
    for (i = 0u; i < 8u; ++i) {
        const unsigned int round = i + 4u;
        ascon_round(
            state,
            (uint8_t)(((15u - round) << 4u) | round)
        );
    }
}

static void initialize(
    ascon_state *state,
    const uint8_t key[16],
    const uint8_t nonce[16]
)
{
    const uint64_t key0 = load_little_endian(key, 8u);
    const uint64_t key1 = load_little_endian(key + 8u, 8u);
    state->x[0] = UINT64_C(0x00001000808c0001);
    state->x[1] = key0;
    state->x[2] = key1;
    state->x[3] = load_little_endian(nonce, 8u);
    state->x[4] = load_little_endian(nonce + 8u, 8u);
    ascon_permute12(state);
    state->x[3] ^= key0;
    state->x[4] ^= key1;
}

static void absorb_associated_data(
    ascon_state *state,
    const uint8_t *data,
    size_t length
)
{
    if (length != 0u) {
        while (length >= 16u) {
            state->x[0] ^= load_little_endian(data, 8u);
            state->x[1] ^= load_little_endian(data + 8u, 8u);
            ascon_permute8(state);
            data += 16u;
            length -= 16u;
        }
        if (length >= 8u) {
            state->x[0] ^= load_little_endian(data, 8u);
            state->x[1] ^= load_little_endian(data + 8u, length - 8u);
            state->x[1] ^= UINT64_C(1) << (8u * (length - 8u));
        } else {
            state->x[0] ^= load_little_endian(data, length);
            state->x[0] ^= UINT64_C(1) << (8u * length);
        }
        ascon_permute8(state);
    }
    state->x[4] ^= UINT64_C(0x8000000000000000);
}

static void finalize_tag(
    ascon_state *state,
    const uint8_t key[16],
    uint8_t tag[16]
)
{
    const uint64_t key0 = load_little_endian(key, 8u);
    const uint64_t key1 = load_little_endian(key + 8u, 8u);
    state->x[2] ^= key0;
    state->x[3] ^= key1;
    ascon_permute12(state);
    state->x[3] ^= key0;
    state->x[4] ^= key1;
    store_little_endian(tag, state->x[3], 8u);
    store_little_endian(tag + 8u, state->x[4], 8u);
}

void npq_ascon_aead128_encrypt(
    uint8_t *ciphertext,
    uint8_t tag[16],
    const uint8_t key[16],
    const uint8_t nonce[16],
    const uint8_t *associated_data,
    size_t associated_data_length,
    const uint8_t *plaintext,
    size_t plaintext_length
)
{
    ascon_state state;
    initialize(&state, key, nonce);
    absorb_associated_data(&state, associated_data, associated_data_length);
    while (plaintext_length >= 16u) {
        state.x[0] ^= load_little_endian(plaintext, 8u);
        state.x[1] ^= load_little_endian(plaintext + 8u, 8u);
        store_little_endian(ciphertext, state.x[0], 8u);
        store_little_endian(ciphertext + 8u, state.x[1], 8u);
        ascon_permute8(&state);
        plaintext += 16u;
        ciphertext += 16u;
        plaintext_length -= 16u;
    }
    if (plaintext_length >= 8u) {
        state.x[0] ^= load_little_endian(plaintext, 8u);
        state.x[1] ^= load_little_endian(
            plaintext + 8u,
            plaintext_length - 8u
        );
        store_little_endian(ciphertext, state.x[0], 8u);
        store_little_endian(
            ciphertext + 8u,
            state.x[1],
            plaintext_length - 8u
        );
        state.x[1] ^= UINT64_C(1) << (8u * (plaintext_length - 8u));
    } else {
        state.x[0] ^= load_little_endian(plaintext, plaintext_length);
        store_little_endian(ciphertext, state.x[0], plaintext_length);
        state.x[0] ^= UINT64_C(1) << (8u * plaintext_length);
    }
    finalize_tag(&state, key, tag);
    npq_secure_zero(&state, sizeof(state));
}

int npq_ascon_aead128_decrypt(
    uint8_t *plaintext,
    const uint8_t key[16],
    const uint8_t nonce[16],
    const uint8_t *associated_data,
    size_t associated_data_length,
    const uint8_t *ciphertext,
    size_t ciphertext_length,
    const uint8_t tag[16]
)
{
    ascon_state state;
    uint8_t expected_tag[16];
    uint8_t *plaintext_start = plaintext;
    const size_t original_length = ciphertext_length;
    initialize(&state, key, nonce);
    absorb_associated_data(&state, associated_data, associated_data_length);
    while (ciphertext_length >= 16u) {
        const uint64_t c0 = load_little_endian(ciphertext, 8u);
        const uint64_t c1 = load_little_endian(ciphertext + 8u, 8u);
        store_little_endian(plaintext, state.x[0] ^ c0, 8u);
        store_little_endian(plaintext + 8u, state.x[1] ^ c1, 8u);
        state.x[0] = c0;
        state.x[1] = c1;
        ascon_permute8(&state);
        plaintext += 16u;
        ciphertext += 16u;
        ciphertext_length -= 16u;
    }
    if (ciphertext_length >= 8u) {
        const uint64_t c0 = load_little_endian(ciphertext, 8u);
        const uint64_t c1 = load_little_endian(
            ciphertext + 8u,
            ciphertext_length - 8u
        );
        store_little_endian(plaintext, state.x[0] ^ c0, 8u);
        store_little_endian(
            plaintext + 8u,
            state.x[1] ^ c1,
            ciphertext_length - 8u
        );
        state.x[0] = c0;
        state.x[1] = clear_low_bytes(
            state.x[1],
            ciphertext_length - 8u
        ) | c1;
        state.x[1] ^= UINT64_C(1) << (8u * (ciphertext_length - 8u));
    } else {
        const uint64_t c0 = load_little_endian(
            ciphertext,
            ciphertext_length
        );
        store_little_endian(
            plaintext,
            state.x[0] ^ c0,
            ciphertext_length
        );
        state.x[0] = clear_low_bytes(state.x[0], ciphertext_length) | c0;
        state.x[0] ^= UINT64_C(1) << (8u * ciphertext_length);
    }
    finalize_tag(&state, key, expected_tag);
    if (!npq_constant_time_equal(expected_tag, tag, sizeof(expected_tag))) {
        npq_secure_zero(plaintext_start, original_length);
        npq_secure_zero(expected_tag, sizeof(expected_tag));
        npq_secure_zero(&state, sizeof(state));
        return NPQ_ERR_AUTH;
    }
    npq_secure_zero(expected_tag, sizeof(expected_tag));
    npq_secure_zero(&state, sizeof(state));
    return NPQ_OK;
}
