#ifndef C0PQLINK_ASCON_AEAD128_H
#define C0PQLINK_ASCON_AEAD128_H

#include "c0pqlink/common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define C0_ASCON_AEAD128_KEY_BYTES 16u
#define C0_ASCON_AEAD128_NONCE_BYTES 16u
#define C0_ASCON_AEAD128_TAG_BYTES 16u

void c0_ascon_aead128_encrypt(
    uint8_t *ciphertext,
    uint8_t tag[C0_ASCON_AEAD128_TAG_BYTES],
    const uint8_t key[C0_ASCON_AEAD128_KEY_BYTES],
    const uint8_t nonce[C0_ASCON_AEAD128_NONCE_BYTES],
    const uint8_t *associated_data,
    size_t associated_data_length,
    const uint8_t *plaintext,
    size_t plaintext_length
);

int c0_ascon_aead128_decrypt(
    uint8_t *plaintext,
    const uint8_t key[C0_ASCON_AEAD128_KEY_BYTES],
    const uint8_t nonce[C0_ASCON_AEAD128_NONCE_BYTES],
    const uint8_t *associated_data,
    size_t associated_data_length,
    const uint8_t *ciphertext,
    size_t ciphertext_length,
    const uint8_t tag[C0_ASCON_AEAD128_TAG_BYTES]
);

#ifdef __cplusplus
}
#endif

#endif

