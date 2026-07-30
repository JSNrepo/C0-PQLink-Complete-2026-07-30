#ifndef QP_SLHDSA_H
#define QP_SLHDSA_H

#include <stddef.h>
#include <stdint.h>

#define QP_SLHDSA_PUBLIC_KEY_BYTES 32U
#define QP_SLHDSA_SIGNATURE_BYTES 7856U

typedef int (*qp_slhdsa_read_fn)(void *context, uint8_t *out, size_t len);

typedef enum {
  QP_SLHDSA_OK = 0,
  QP_SLHDSA_BAD_SIGNATURE = 1,
  QP_SLHDSA_BAD_LENGTH = 2,
  QP_SLHDSA_BAD_CONTEXT = 3,
  QP_SLHDSA_IO_ERROR = 4
} qp_slhdsa_result;

/*
 * One-pass verifier for the FIPS 205 external SLH-DSA-SHA2-128s interface.
 *
 * The 7,856-byte signature is consumed in wire order through reader and is
 * never materialized in SRAM.  Context lengths from 0 through 255 are
 * supported, as required by the external interface.
 */
qp_slhdsa_result qp_slhdsa_sha2_128s_verify_stream(
    const uint8_t public_key[QP_SLHDSA_PUBLIC_KEY_BYTES],
    const uint8_t *message, size_t message_len, const uint8_t *context,
    size_t context_len, size_t signature_len, qp_slhdsa_read_fn reader,
    void *reader_context, size_t *bytes_consumed);

#endif
