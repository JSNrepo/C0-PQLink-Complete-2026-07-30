#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hss_verify.h"

typedef struct {
    const char *name;
    const char *public_path;
    const char *signature_path;
    size_t signature_length;
} reference_profile;

static int read_exact(const char *path, uint8_t *output, size_t length)
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

static int verify_profile(
    const reference_profile *profile,
    const uint8_t message[48]
)
{
    uint8_t public_key[60] = { 0u, 0u, 0u, 1u };
    uint8_t signature[2352] = { 0u, 0u, 0u, 0u };
    const size_t hss_signature_length = profile->signature_length + 4u;
    if (!read_exact(profile->public_path, public_key + 4u, 56u)
        || !read_exact(
            profile->signature_path,
            signature + 4u,
            profile->signature_length
        )) {
        fprintf(stderr, "FAIL: could not load %s inputs\n", profile->name);
        return 0;
    }
    if (!hss_validate_signature(
            public_key,
            message,
            48u,
            signature,
            hss_signature_length,
            NULL
        )) {
        fprintf(
            stderr,
            "FAIL: Cisco RFC 8554 implementation rejected %s\n",
            profile->name
        );
        return 0;
    }
    signature[hss_signature_length - 1u] ^= 1u;
    if (hss_validate_signature(
            public_key,
            message,
            48u,
            signature,
            hss_signature_length,
            NULL
        )) {
        fprintf(
            stderr,
            "FAIL: Cisco implementation accepted %s tamper\n",
            profile->name
        );
        return 0;
    }
    printf(
        "PASS: Cisco RFC 8554 reference accepted %s and rejected tamper\n",
        profile->name
    );
    return 1;
}

int main(void)
{
    static const reference_profile profiles[] = {
        {
            "LMS H5/W4",
            "tests/vectors/lms_w4_public.bin",
            "tests/vectors/lms_w4_signature.bin",
            2348u
        },
        {
            "LMS H5/W8",
            "tests/vectors/lms_w8_public.bin",
            "tests/vectors/lms_w8_signature.bin",
            1292u
        }
    };
    uint8_t message[48];
    size_t index;
    int passed = 1;
    if (!read_exact(
            "tests/vectors/cutover_manifest.bin",
            message,
            sizeof(message)
        )) {
        fprintf(stderr, "FAIL: could not load reference message\n");
        return 2;
    }
    for (index = 0u; index < sizeof(profiles) / sizeof(profiles[0]); ++index) {
        passed &= verify_profile(&profiles[index], message);
    }
    return passed ? 0 : 1;
}
