#include "slhdsa.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const uint8_t *bytes;
    size_t length;
    size_t offset;
    uint8_t tamper;
} memory_reader;

static int read_signature(void *context, uint8_t *output, size_t length)
{
    memory_reader *reader = (memory_reader *)context;
    if (reader->offset > reader->length
        || length > reader->length - reader->offset) {
        return 0;
    }
    memcpy(output, reader->bytes + reader->offset, length);
    if (reader->tamper != 0u
        && reader->offset <= QP_SLHDSA_SIGNATURE_BYTES - 1u
        && QP_SLHDSA_SIGNATURE_BYTES - 1u - reader->offset < length) {
        output[
            QP_SLHDSA_SIGNATURE_BYTES - 1u - reader->offset
        ] ^= 1u;
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

static qp_slhdsa_result verify(
    const uint8_t public_key[QP_SLHDSA_PUBLIC_KEY_BYTES],
    const uint8_t manifest[48],
    const uint8_t signature[QP_SLHDSA_SIGNATURE_BYTES],
    size_t available,
    uint8_t tamper,
    size_t *consumed
)
{
    static const uint8_t context[] = "Q-PUNCTURE-205";
    memory_reader reader;
    reader.bytes = signature;
    reader.length = available;
    reader.offset = 0u;
    reader.tamper = tamper;
    return qp_slhdsa_sha2_128s_verify_stream(
        public_key,
        manifest,
        48u,
        context,
        sizeof(context) - 1u,
        QP_SLHDSA_SIGNATURE_BYTES,
        read_signature,
        &reader,
        consumed
    );
}

int main(void)
{
    uint8_t public_key[QP_SLHDSA_PUBLIC_KEY_BYTES];
    uint8_t manifest[48];
    uint8_t *signature = (uint8_t *)malloc(QP_SLHDSA_SIGNATURE_BYTES);
    size_t valid_consumed = 0u;
    size_t tamper_consumed = 0u;
    size_t truncated_consumed = 0u;
    qp_slhdsa_result valid;
    qp_slhdsa_result tampered;
    qp_slhdsa_result truncated;
    int passed;
    if (signature == NULL
        || !read_exact_file(
            "tests/vectors/slhdsa_public.bin",
            public_key,
            sizeof(public_key)
        )
        || !read_exact_file(
            "tests/vectors/cutover_manifest.bin",
            manifest,
            sizeof(manifest)
        )
        || !read_exact_file(
            "tests/vectors/slhdsa_signature.bin",
            signature,
            QP_SLHDSA_SIGNATURE_BYTES
        )) {
        fprintf(stderr, "FAIL: could not load exact SLH-DSA vectors\n");
        free(signature);
        return 2;
    }
    valid = verify(
        public_key,
        manifest,
        signature,
        QP_SLHDSA_SIGNATURE_BYTES,
        0u,
        &valid_consumed
    );
    tampered = verify(
        public_key,
        manifest,
        signature,
        QP_SLHDSA_SIGNATURE_BYTES,
        1u,
        &tamper_consumed
    );
    truncated = verify(
        public_key,
        manifest,
        signature,
        QP_SLHDSA_SIGNATURE_BYTES - 1u,
        0u,
        &truncated_consumed
    );
    passed = valid == QP_SLHDSA_OK
        && valid_consumed == QP_SLHDSA_SIGNATURE_BYTES
        && tampered == QP_SLHDSA_BAD_SIGNATURE
        && tamper_consumed == QP_SLHDSA_SIGNATURE_BYTES
        && truncated == QP_SLHDSA_IO_ERROR
        && truncated_consumed < QP_SLHDSA_SIGNATURE_BYTES;
    printf(
        "%s: FIPS 205 SLH-DSA-SHA2-128s valid vector, tamper, "
        "truncation, and %u-byte streaming consumption\n",
        passed ? "PASS" : "FAIL",
        QP_SLHDSA_SIGNATURE_BYTES
    );
    memset(signature, 0, QP_SLHDSA_SIGNATURE_BYTES);
    free(signature);
    return passed ? 0 : 1;
}
