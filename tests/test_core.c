#include "c0pqlink/mlkem512_stream.h"
#include "c0pqlink/ascon_aead128.h"
#include "c0pqlink/sha256.h"

#include "core/keccak.h"

#include <stdio.h>
#include <string.h>

static int expect_hex(
    const char *name,
    const uint8_t *actual,
    const uint8_t *expected,
    size_t length
)
{
    if (!c0pqlink_ct_equal(actual, expected, length)) {
        fprintf(stderr, "FAIL: %s\n", name);
        return 1;
    }
    return 0;
}

int test_core(void)
{
    static const uint8_t sha3_empty[32] = {
        0xa7, 0xff, 0xc6, 0xf8, 0xbf, 0x1e, 0xd7, 0x66,
        0x51, 0xc1, 0x47, 0x56, 0xa0, 0x61, 0xd6, 0x62,
        0xf5, 0x80, 0xff, 0x4d, 0xe4, 0x3b, 0x49, 0xfa,
        0x82, 0xd8, 0x0a, 0x4b, 0x80, 0xf8, 0x43, 0x4a
    };
    uint8_t output[32];
    static const uint8_t sha256_abc[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    static const uint8_t hmac_expected[32] = {
        0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53,
        0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0x0b, 0xf1, 0x2b,
        0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83, 0x3d, 0xa7,
        0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7
    };
    static const uint8_t hkdf_prk_expected[32] = {
        0x07, 0x77, 0x09, 0x36, 0x2c, 0x2e, 0x32, 0xdf,
        0x0d, 0xdc, 0x3f, 0x0d, 0xc4, 0x7b, 0xba, 0x63,
        0x90, 0xb6, 0xc7, 0x3b, 0xb5, 0x0f, 0x9c, 0x31,
        0x22, 0xec, 0x84, 0x4a, 0xd7, 0xc2, 0xb3, 0xe5
    };
    static const uint8_t hkdf_okm_expected[42] = {
        0x3c, 0xb2, 0x5f, 0x25, 0xfa, 0xac, 0xd5, 0x7a,
        0x90, 0x43, 0x4f, 0x64, 0xd0, 0x36, 0x2f, 0x2a,
        0x2d, 0x2d, 0x0a, 0x90, 0xcf, 0x1a, 0x5a, 0x4c,
        0x5d, 0xb0, 0x2d, 0x56, 0xec, 0xc4, 0xc5, 0xbf,
        0x34, 0x00, 0x72, 0x08, 0xd5, 0xb8, 0x87, 0x18,
        0x58, 0x65
    };
    static const uint8_t ascon_expected[31] = {
        0xb3, 0xb0, 0x5f, 0x0a, 0x08, 0xc5, 0x76, 0x97,
        0x49, 0xa3, 0xb5, 0x65, 0x8b, 0xcb, 0x1d,
        0x9c, 0x4c, 0x29, 0x70, 0xaa, 0x15, 0x4b, 0x85,
        0x47, 0xc7, 0xd0, 0xfd, 0x2a, 0xd4, 0x55, 0xa0
    };
    uint8_t key[16];
    uint8_t nonce[16];
    uint8_t plaintext[15];
    uint8_t associated_data[18];
    uint8_t ciphertext[15];
    uint8_t decrypted[15];
    uint8_t tag[16];
    uint8_t hmac_key[20];
    uint8_t hkdf_ikm[22];
    uint8_t hkdf_salt[13];
    uint8_t hkdf_info[10];
    uint8_t hkdf_prk[32];
    uint8_t hkdf_okm[42];
    unsigned int i;
    int failures = 0;
    c0_sha3_256(output, NULL, 0u);
    failures += expect_hex("SHA3-256 empty", output, sha3_empty, 32u);
    c0_sha256(output, (const uint8_t *)"abc", 3u);
    failures += expect_hex("SHA-256 abc", output, sha256_abc, 32u);
    memset(hmac_key, 0x0b, sizeof(hmac_key));
    c0_hmac_sha256(
        output,
        hmac_key,
        sizeof(hmac_key),
        (const uint8_t *)"Hi There",
        8u
    );
    failures += expect_hex(
        "HMAC-SHA-256 RFC 4231",
        output,
        hmac_expected,
        sizeof(hmac_expected)
    );
    memset(hkdf_ikm, 0x0b, sizeof(hkdf_ikm));
    for (i = 0u; i < sizeof(hkdf_salt); ++i) {
        hkdf_salt[i] = (uint8_t)i;
    }
    for (i = 0u; i < sizeof(hkdf_info); ++i) {
        hkdf_info[i] = (uint8_t)(0xf0u + i);
    }
    c0_hkdf_sha256_extract(
        hkdf_prk,
        hkdf_salt,
        sizeof(hkdf_salt),
        hkdf_ikm,
        sizeof(hkdf_ikm)
    );
    failures += expect_hex(
        "HKDF-SHA-256 PRK RFC 5869",
        hkdf_prk,
        hkdf_prk_expected,
        sizeof(hkdf_prk_expected)
    );
    if (c0_hkdf_sha256_expand(
            hkdf_okm,
            sizeof(hkdf_okm),
            hkdf_prk,
            hkdf_info,
            sizeof(hkdf_info)
        ) != C0PQLINK_OK) {
        fprintf(stderr, "FAIL: HKDF-SHA-256 expand status\n");
        ++failures;
    }
    failures += expect_hex(
        "HKDF-SHA-256 OKM RFC 5869",
        hkdf_okm,
        hkdf_okm_expected,
        sizeof(hkdf_okm_expected)
    );
    for (i = 0u; i < 18u; ++i) {
        if (i < 16u) {
            key[i] = (uint8_t)i;
            nonce[i] = (uint8_t)(0x10u + i);
        }
        if (i < 15u) {
            plaintext[i] = (uint8_t)(0x20u + i);
        }
        associated_data[i] = (uint8_t)(0x30u + i);
    }
    c0_ascon_aead128_encrypt(
        ciphertext,
        tag,
        key,
        nonce,
        associated_data,
        sizeof(associated_data),
        plaintext,
        sizeof(plaintext)
    );
    failures += expect_hex(
        "Ascon-AEAD128 ciphertext",
        ciphertext,
        ascon_expected,
        sizeof(ciphertext)
    );
    failures += expect_hex(
        "Ascon-AEAD128 tag",
        tag,
        ascon_expected + sizeof(ciphertext),
        sizeof(tag)
    );
    if (c0_ascon_aead128_decrypt(
            decrypted,
            key,
            nonce,
            associated_data,
            sizeof(associated_data),
            ciphertext,
            sizeof(ciphertext),
            tag
        ) != C0PQLINK_OK
        || !c0pqlink_ct_equal(decrypted, plaintext, sizeof(plaintext))) {
        fprintf(stderr, "FAIL: Ascon-AEAD128 decrypt\n");
        ++failures;
    }
    tag[0] ^= 1u;
    memset(decrypted, 0xa5, sizeof(decrypted));
    if (c0_ascon_aead128_decrypt(
            decrypted,
            key,
            nonce,
            associated_data,
            sizeof(associated_data),
            ciphertext,
            sizeof(ciphertext),
            tag
        ) != C0PQLINK_ERR_AUTH) {
        fprintf(stderr, "FAIL: Ascon-AEAD128 tamper rejection\n");
        ++failures;
    }
    for (i = 0u; i < sizeof(decrypted); ++i) {
        if (decrypted[i] != 0u) {
            fprintf(stderr, "FAIL: Ascon-AEAD128 failure zeroization\n");
            ++failures;
            break;
        }
    }
    if (c0_mlkem512_workspace_bytes() > C0_MLKEM512_WORKSPACE_BYTES) {
        fprintf(stderr, "FAIL: ML-KEM workspace bound\n");
        ++failures;
    }
    return failures;
}
