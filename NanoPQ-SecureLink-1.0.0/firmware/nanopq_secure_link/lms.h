#ifndef NPQ_LMS_H
#define NPQ_LMS_H

#include <stddef.h>
#include <stdint.h>

#define NPQ_LMS_PUBLIC_KEY_BYTES 56u
#define NPQ_LMS_W4_SIGNATURE_BYTES 2348u
#define NPQ_LMS_W8_SIGNATURE_BYTES 1292u

typedef int (*npq_lms_read_fn)(
    void *context,
    uint8_t *output,
    size_t length
);

typedef enum {
    NPQ_LMS_OK = 0,
    NPQ_LMS_BAD_SIGNATURE = 1,
    NPQ_LMS_BAD_LENGTH = 2,
    NPQ_LMS_BAD_PARAMETER = 3,
    NPQ_LMS_IO_ERROR = 4
} npq_lms_result;

/*
 * Constant-memory RFC 8554 verifier specialized to the NIST SP 800-208
 * LMS_SHA256_M32_H5 and the NIST-approved LMOTS_SHA256_N32_W4/W8
 * time-versus-wire-size profiles.
 *
 * The signer is stateful; that state belongs to the off-device provisioning
 * authority. Verification on the Nano does not maintain an LMS private-key
 * counter. The 1,292-byte signature is consumed through reader.
 */
npq_lms_result npq_lms_sha256_m32_h5_verify_stream(
    const uint8_t public_key[NPQ_LMS_PUBLIC_KEY_BYTES],
    const uint8_t *message,
    size_t message_length,
    size_t signature_length,
    npq_lms_read_fn reader,
    void *reader_context,
    size_t *bytes_consumed
);

#endif
