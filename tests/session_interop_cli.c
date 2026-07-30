#include "c0pqlink/c0pqlink.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t public_key[C0_MLKEM512_PUBLIC_KEY_BYTES];
    uint32_t rng_state;
} test_context;

static int hex_value(int character)
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

static int decode_hex(const char *input, uint8_t *output, size_t length)
{
    size_t i;
    if (strlen(input) != length * 2u) {
        return -1;
    }
    for (i = 0u; i < length; ++i) {
        const int high = hex_value((unsigned char)input[2u * i]);
        const int low = hex_value((unsigned char)input[2u * i + 1u]);
        if (high < 0 || low < 0) {
            return -1;
        }
        output[i] = (uint8_t)((unsigned int)high << 4u
            | (unsigned int)low);
    }
    return 0;
}

static void print_hex(const uint8_t *bytes, size_t length)
{
    static const char alphabet[] = "0123456789abcdef";
    size_t i;
    for (i = 0u; i < length; ++i) {
        putchar(alphabet[bytes[i] >> 4u]);
        putchar(alphabet[bytes[i] & 15u]);
    }
}

static uint8_t read_public_key(void *context, uint16_t offset)
{
    test_context *test = (test_context *)context;
    return test->public_key[offset];
}

static int test_random(void *context, uint8_t *output, size_t length)
{
    test_context *test = (test_context *)context;
    size_t i;
    for (i = 0u; i < length; ++i) {
        uint32_t value = test->rng_state;
        value ^= value << 13u;
        value ^= value >> 17u;
        value ^= value << 5u;
        test->rng_state = value;
        output[i] = (uint8_t)value;
    }
    return 0;
}

static int send_frame(
    void *context,
    const uint8_t *frame,
    size_t frame_length
)
{
    (void)context;
    fputs("TX ", stdout);
    print_hex(frame, frame_length);
    putchar('\n');
    return fflush(stdout) == 0 ? 0 : -1;
}

static int receive_frame(
    void *context,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *frame_length,
    uint32_t timeout_ms
)
{
    char line[2u * C0PQ_FRAME_MAX_BYTES + 16u];
    char *hex;
    size_t length;
    (void)context;
    (void)timeout_ms;
    if (fgets(line, sizeof(line), stdin) == NULL) {
        return -1;
    }
    if (strncmp(line, "TIMEOUT", 7u) == 0) {
        return -1;
    }
    if (strncmp(line, "RX ", 3u) != 0) {
        return -1;
    }
    hex = line + 3u;
    length = strcspn(hex, "\r\n");
    hex[length] = '\0';
    if ((length & 1u) != 0u || length / 2u > frame_capacity
        || decode_hex(hex, frame, length / 2u) != 0) {
        return -1;
    }
    *frame_length = length / 2u;
    return 0;
}

int main(int argc, char **argv)
{
    static const uint8_t device_id[C0PQ_DEVICE_ID_BYTES] = {
        1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u
    };
    static const uint8_t sensor_message[] = "temperature=24.5";
    test_context test;
    c0pq_client_config config;
    c0pq_client client;
    c0_mlkem512_workspace workspace;
    uint8_t outgoing[C0PQ_FRAME_MAX_BYTES];
    uint8_t incoming[C0PQ_FRAME_MAX_BYTES];
    uint8_t plaintext[C0PQ_RECORD_PLAINTEXT_MAX];
    size_t outgoing_length = 0u;
    size_t incoming_length = 0u;
    size_t plaintext_length = 0u;
    unsigned int attempt;
    int result;
    if (argc != 5) {
        fprintf(stderr, "usage: %s PK_HEX PSK_HEX KEY_ID_HEX MODE\n", argv[0]);
        return 2;
    }
    memset(&test, 0, sizeof(test));
    test.rng_state = UINT32_C(0x6d2b79f5);
    memset(&config, 0, sizeof(config));
    if (decode_hex(
            argv[1],
            test.public_key,
            sizeof(test.public_key)
        ) != 0
        || decode_hex(argv[2], config.psk, sizeof(config.psk)) != 0
        || decode_hex(
            argv[3],
            config.public_key_id,
            sizeof(config.public_key_id)
        ) != 0) {
        fputs("invalid hex argument\n", stderr);
        return 2;
    }
    memcpy(config.device_id, device_id, sizeof(device_id));
    config.epoch = 1u;
    config.read_public_key = read_public_key;
    config.public_key_context = &test;
    config.random_bytes = test_random;
    config.rng_context = &test;
    config.send_frame = send_frame;
    config.receive_frame = receive_frame;
    config.transport_context = &test;
    config.mode = atoi(argv[4]) == 0
        ? C0PQ_FULL_PQ_EACH_SESSION : C0PQ_PQ_BOOTSTRAP_RATCHET;
    config.timeout_ms = 25u;
    config.maximum_retries = 4u;
    result = c0pq_client_init(&client, &config);
    if (result == C0PQLINK_OK) {
        result = c0pq_client_connect(&client, &workspace);
    }
    if (result != C0PQLINK_OK) {
        fprintf(stderr, "connect failed: %d\n", result);
        return 1;
    }
    result = c0pq_client_seal_record(
        &client,
        sensor_message,
        sizeof(sensor_message) - 1u,
        outgoing,
        sizeof(outgoing),
        &outgoing_length
    );
    for (attempt = 0u;
         result == C0PQLINK_OK
            && attempt <= (unsigned int)config.maximum_retries;
         ++attempt) {
        result = send_frame(&test, outgoing, outgoing_length);
        if (result != C0PQLINK_OK) {
            result = C0PQLINK_OK;
            continue;
        }
        result = receive_frame(
            &test,
            incoming,
            sizeof(incoming),
            &incoming_length,
            25u
        );
        if (result != C0PQLINK_OK) {
            result = C0PQLINK_OK;
            continue;
        }
        result = c0pq_client_open_record(
            &client,
            incoming,
            incoming_length,
            plaintext,
            sizeof(plaintext),
            &plaintext_length
        );
        if (result == C0PQLINK_OK) {
            break;
        }
    }
    if (attempt > (unsigned int)config.maximum_retries
        && result == C0PQLINK_OK
        && plaintext_length == 0u) {
        result = C0PQLINK_ERR_IO;
    }
    if (result != C0PQLINK_OK) {
        fprintf(stderr, "record exchange failed: %d\n", result);
        return 1;
    }
    fputs("DONE ", stdout);
    print_hex(plaintext, plaintext_length);
    putchar('\n');
    fflush(stdout);
    c0pq_client_close(&client);
    return 0;
}
