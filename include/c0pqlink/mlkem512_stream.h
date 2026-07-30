#ifndef C0PQLINK_MLKEM512_STREAM_H
#define C0PQLINK_MLKEM512_STREAM_H

#include "c0pqlink/common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define C0_MLKEM512_PUBLIC_KEY_BYTES 800u
#define C0_MLKEM512_CIPHERTEXT_BYTES 768u
#define C0_MLKEM512_SHARED_SECRET_BYTES 32u
#define C0_MLKEM512_RANDOM_BYTES 32u

/*
 * This bound is intentionally public so Class-0 firmware can allocate one
 * phase-overlaid workspace statically. The implementation checks at compile
 * time that its internal state fits.
 */
#define C0_MLKEM512_WORKSPACE_BYTES 1344u

typedef union {
    uint64_t align_u64;
    uint32_t align_u32;
    uint8_t bytes[C0_MLKEM512_WORKSPACE_BYTES];
} c0_mlkem512_workspace;

typedef uint8_t (*c0_mlkem512_pk_read_fn)(void *context, uint16_t offset);
typedef int (*c0_mlkem512_ct_write_fn)(
    void *context,
    const uint8_t *data,
    size_t length
);
typedef int (*c0pqlink_rng_fn)(void *context, uint8_t *output, size_t length);

/*
 * FIPS 203 ML-KEM-512 encapsulation with a callback-backed public key and
 * streaming ciphertext. `randomness` is Algorithm 17's 32-byte random input.
 *
 * The function preserves ML-KEM mathematics and wire bytes. It changes only
 * the execution schedule: public-key polynomials and ephemeral polynomials are
 * regenerated/read as required so the whole key or ciphertext never occupies
 * SRAM.
 */
int c0_mlkem512_encapsulate_derand(
    c0_mlkem512_pk_read_fn read_public_key,
    void *public_key_context,
    const uint8_t randomness[C0_MLKEM512_RANDOM_BYTES],
    c0_mlkem512_ct_write_fn write_ciphertext,
    void *ciphertext_context,
    uint8_t shared_secret[C0_MLKEM512_SHARED_SECRET_BYTES],
    c0_mlkem512_workspace *workspace
);

int c0_mlkem512_encapsulate(
    c0_mlkem512_pk_read_fn read_public_key,
    void *public_key_context,
    c0pqlink_rng_fn random_bytes,
    void *rng_context,
    c0_mlkem512_ct_write_fn write_ciphertext,
    void *ciphertext_context,
    uint8_t shared_secret[C0_MLKEM512_SHARED_SECRET_BYTES],
    c0_mlkem512_workspace *workspace
);

size_t c0_mlkem512_workspace_bytes(void);

#ifdef __cplusplus
}
#endif

#endif
