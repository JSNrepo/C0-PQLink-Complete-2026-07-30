#include "lms.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const uint8_t *bytes;
    size_t length;
    size_t offset;
    size_t tamper_at;
    uint8_t tamper;
} memory_reader;

typedef struct {
    const char *name;
    const char *public_path;
    const char *signature_path;
    size_t signature_length;
} lms_profile;

static int read_signature(void *context, uint8_t *output, size_t length)
{
    memory_reader *reader = (memory_reader *)context;
    if (reader->offset > reader->length
        || length > reader->length - reader->offset) {
        return 0;
    }
    memcpy(output, reader->bytes + reader->offset, length);
    if (reader->tamper != 0u
        && reader->offset <= reader->tamper_at
        && reader->tamper_at - reader->offset < length) {
        output[reader->tamper_at - reader->offset] ^= 1u;
    }
    reader->offset += length;
    return 1;
}

static int read_exact_file(
    const char *path,
    uint8_t *output,
    size_t length
)
{
    FILE *handle = fopen(path, "rb");
    int trailing;
    if (handle == NULL
        || fread(output, 1u, length, handle) != length) {
        if (handle != NULL) {
            fclose(handle);
        }
        return 0;
    }
    trailing = fgetc(handle);
    fclose(handle);
    return trailing == EOF;
}

static npq_lms_result verify(
    const uint8_t public_key[NPQ_LMS_PUBLIC_KEY_BYTES],
    const uint8_t message[48],
    const uint8_t *signature,
    size_t signature_length,
    size_t available,
    uint8_t tamper,
    size_t *consumed
)
{
    memory_reader reader;
    reader.bytes = signature;
    reader.length = available;
    reader.offset = 0u;
    reader.tamper_at = signature_length - 1u;
    reader.tamper = tamper;
    return npq_lms_sha256_m32_h5_verify_stream(
        public_key,
        message,
        48u,
        signature_length,
        read_signature,
        &reader,
        consumed
    );
}

static int test_profile(
    const lms_profile *profile,
    const uint8_t message[48]
)
{
    uint8_t public_key[NPQ_LMS_PUBLIC_KEY_BYTES];
    uint8_t *signature = (uint8_t *)malloc(profile->signature_length);
    size_t valid_consumed = 0u;
    size_t tamper_consumed = 0u;
    size_t truncated_consumed = 0u;
    npq_lms_result valid;
    npq_lms_result tampered;
    npq_lms_result truncated;
    int passed;
    if (signature == NULL
        || !read_exact_file(
            profile->public_path,
            public_key,
            sizeof(public_key)
        )
        || !read_exact_file(
            profile->signature_path,
            signature,
            profile->signature_length
        )) {
        fprintf(stderr, "FAIL: could not load %s vectors\n", profile->name);
        free(signature);
        return 0;
    }
    valid = verify(
        public_key,
        message,
        signature,
        profile->signature_length,
        profile->signature_length,
        0u,
        &valid_consumed
    );
    tampered = verify(
        public_key,
        message,
        signature,
        profile->signature_length,
        profile->signature_length,
        1u,
        &tamper_consumed
    );
    truncated = verify(
        public_key,
        message,
        signature,
        profile->signature_length,
        profile->signature_length - 1u,
        0u,
        &truncated_consumed
    );
    passed = valid == NPQ_LMS_OK
        && valid_consumed == profile->signature_length
        && tampered == NPQ_LMS_BAD_SIGNATURE
        && tamper_consumed == profile->signature_length
        && truncated == NPQ_LMS_IO_ERROR
        && truncated_consumed < profile->signature_length;
    printf(
        "%s: %s valid vector, tamper, truncation, and %zu-byte "
        "streaming consumption\n",
        passed ? "PASS" : "FAIL",
        profile->name,
        profile->signature_length
    );
    memset(signature, 0, profile->signature_length);
    free(signature);
    return passed;
}

int main(void)
{
    static const lms_profile profiles[] = {
        {
            "RFC 8554 LMS H5/W4",
            "tests/vectors/lms_w4_public.bin",
            "tests/vectors/lms_w4_signature.bin",
            NPQ_LMS_W4_SIGNATURE_BYTES
        },
        {
            "RFC 8554 LMS H5/W8",
            "tests/vectors/lms_w8_public.bin",
            "tests/vectors/lms_w8_signature.bin",
            NPQ_LMS_W8_SIGNATURE_BYTES
        }
    };
    uint8_t message[48];
    size_t index;
    int passed = 1;
    if (!read_exact_file(
            "tests/vectors/cutover_manifest.bin",
            message,
            sizeof(message)
        )) {
        fprintf(stderr, "FAIL: could not load LMS message\n");
        return 2;
    }
    for (index = 0u; index < sizeof(profiles) / sizeof(profiles[0]); ++index) {
        passed &= test_profile(&profiles[index], message);
    }
    return passed ? 0 : 1;
}
