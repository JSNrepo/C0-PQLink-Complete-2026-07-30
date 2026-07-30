#include "c0pqlink/mlkem512_stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t bytes[C0_MLKEM512_PUBLIC_KEY_BYTES];
} public_key_buffer;

typedef struct {
    uint8_t bytes[C0_MLKEM512_CIPHERTEXT_BYTES];
    size_t length;
    size_t maximum_write;
} ciphertext_buffer;

static int hex_nibble(char character)
{
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

static int decode_hex(const char *hex, uint8_t *output, size_t length)
{
    size_t i;
    if (strlen(hex) != length * 2u) {
        return -1;
    }
    for (i = 0u; i < length; ++i) {
        const int high = hex_nibble(hex[2u * i]);
        const int low = hex_nibble(hex[2u * i + 1u]);
        if (high < 0 || low < 0) {
            return -1;
        }
        output[i] = (uint8_t)((unsigned int)high << 4u | (unsigned int)low);
    }
    return 0;
}

static void print_hex(const uint8_t *bytes, size_t length)
{
    static const char alphabet[] = "0123456789abcdef";
    size_t i;
    for (i = 0u; i < length; ++i) {
        putchar(alphabet[bytes[i] >> 4u]);
        putchar(alphabet[bytes[i] & 0x0fu]);
    }
}

static uint8_t read_public_key(void *context, uint16_t offset)
{
    public_key_buffer *key = (public_key_buffer *)context;
    return key->bytes[offset];
}

static int write_ciphertext(void *context, const uint8_t *data, size_t length)
{
    ciphertext_buffer *ciphertext = (ciphertext_buffer *)context;
    if (ciphertext->length + length > sizeof(ciphertext->bytes)) {
        return -1;
    }
    memcpy(ciphertext->bytes + ciphertext->length, data, length);
    ciphertext->length += length;
    if (length > ciphertext->maximum_write) {
        ciphertext->maximum_write = length;
    }
    return 0;
}

int main(int argc, char **argv)
{
    public_key_buffer public_key;
    ciphertext_buffer ciphertext;
    c0_mlkem512_workspace workspace;
    uint8_t randomness[C0_MLKEM512_RANDOM_BYTES];
    uint8_t shared_secret[C0_MLKEM512_SHARED_SECRET_BYTES];
    int result;

    if (argc != 3) {
        fprintf(stderr, "usage: %s PUBLIC_KEY_HEX RANDOMNESS_HEX\n", argv[0]);
        return 2;
    }
    if (decode_hex(argv[1], public_key.bytes, sizeof(public_key.bytes)) != 0
        || decode_hex(argv[2], randomness, sizeof(randomness)) != 0) {
        fputs("invalid hex input\n", stderr);
        return 2;
    }
    memset(&ciphertext, 0, sizeof(ciphertext));
    result = c0_mlkem512_encapsulate_derand(
        read_public_key,
        &public_key,
        randomness,
        write_ciphertext,
        &ciphertext,
        shared_secret,
        &workspace
    );
    if (result != C0PQLINK_OK) {
        fprintf(stderr, "encapsulation failed: %d\n", result);
        return 1;
    }
    if (ciphertext.length != C0_MLKEM512_CIPHERTEXT_BYTES) {
        fprintf(stderr, "wrong ciphertext length: %zu\n", ciphertext.length);
        return 1;
    }
    print_hex(ciphertext.bytes, ciphertext.length);
    putchar('\n');
    print_hex(shared_secret, sizeof(shared_secret));
    putchar('\n');
    printf("%zu\n", ciphertext.maximum_write);
    return 0;
}

