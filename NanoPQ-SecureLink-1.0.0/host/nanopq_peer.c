#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "lms.h"
#include "protocol.h"
#include "slhdsa.h"

#define PEER_TIMEOUT_MS 120000
#define PEER_MANIFEST_BYTES 48u
#define PEER_ENROLL_FRAME_BYTES 52u

static const uint8_t demo_root_key[NPQ_ROOT_KEY_BYTES] = {
    0x1b, 0x47, 0x6c, 0xe0, 0x0b, 0x45, 0x87, 0x91,
    0x7e, 0xc4, 0xee, 0x14, 0xa2, 0x5e, 0x1a, 0xf6,
    0xb5, 0x3a, 0xb3, 0x39, 0x6d, 0x6c, 0x28, 0x1f,
    0x5a, 0x7a, 0x84, 0xc7, 0x16, 0x67, 0x33, 0x42
};

static const uint8_t demo_device_id[NPQ_DEVICE_ID_BYTES] = {
    0x03, 0xaf, 0xe9, 0xd9, 0x89, 0x40, 0xf1, 0xd2
};

typedef struct {
    const char *port;
    const char *manifest;
    const char *signature;
    const char *state;
    uint8_t led;
    uint8_t tamper_demo;
} peer_options;

static void usage(const char *program)
{
    fprintf(
        stderr,
        "usage: %s --port DEVICE [--led on|off] [--tamper-demo]\\n"
        "       [--manifest FILE] [--signature FILE] [--state FILE]\\n",
        program
    );
}

static int parse_options(
    int argc,
    char **argv,
    peer_options *options
)
{
    int index;
    options->port = NULL;
    options->manifest = "tests/vectors/cutover_manifest.bin";
    options->signature = NULL;
    options->state = "build/nanopq-peer.state";
    options->led = 1u;
    options->tamper_demo = 0u;
    for (index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--port") == 0 && index + 1 < argc) {
            options->port = argv[++index];
        } else if (strcmp(argv[index], "--manifest") == 0
            && index + 1 < argc) {
            options->manifest = argv[++index];
        } else if (strcmp(argv[index], "--signature") == 0
            && index + 1 < argc) {
            options->signature = argv[++index];
        } else if (strcmp(argv[index], "--state") == 0
            && index + 1 < argc) {
            options->state = argv[++index];
        } else if (strcmp(argv[index], "--led") == 0
            && index + 1 < argc) {
            const char *value = argv[++index];
            if (strcmp(value, "on") == 0) {
                options->led = 1u;
            } else if (strcmp(value, "off") == 0) {
                options->led = 0u;
            } else {
                return 0;
            }
        } else if (strcmp(argv[index], "--tamper-demo") == 0) {
            options->tamper_demo = 1u;
        } else {
            return 0;
        }
    }
    return options->port != NULL;
}

static int wait_fd(int fd, short events, int timeout_ms)
{
    struct pollfd descriptor;
    int result;
    descriptor.fd = fd;
    descriptor.events = events;
    descriptor.revents = 0;
    do {
        result = poll(&descriptor, 1u, timeout_ms);
    } while (result < 0 && errno == EINTR);
    return result > 0
        && (descriptor.revents & (events | POLLERR | POLLHUP)) != 0;
}

static int read_exact(
    int fd,
    uint8_t *output,
    size_t length,
    int timeout_ms
)
{
    size_t offset = 0u;
    while (offset < length) {
        ssize_t received;
        if (!wait_fd(fd, POLLIN, timeout_ms)) {
            return 0;
        }
        received = read(fd, output + offset, length - offset);
        if (received < 0 && errno == EINTR) {
            continue;
        }
        if (received <= 0) {
            return 0;
        }
        offset += (size_t)received;
    }
    return 1;
}

static int write_exact(int fd, const uint8_t *input, size_t length)
{
    size_t offset = 0u;
    while (offset < length) {
        ssize_t written;
        if (!wait_fd(fd, POLLOUT, PEER_TIMEOUT_MS)) {
            return 0;
        }
        written = write(fd, input + offset, length - offset);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            return 0;
        }
        offset += (size_t)written;
    }
    return tcdrain(fd) == 0;
}

static int read_frame(
    int fd,
    uint8_t frame[NPQ_FRAME_MAX_BYTES],
    uint8_t *frame_length
)
{
    uint8_t length;
    if (!read_exact(fd, &length, 1u, PEER_TIMEOUT_MS)
        || length == 0u
        || length > NPQ_FRAME_MAX_BYTES
        || !read_exact(fd, frame, length, PEER_TIMEOUT_MS)) {
        return 0;
    }
    *frame_length = length;
    return 1;
}

static int read_protocol_frame(
    int fd,
    uint8_t frame[NPQ_FRAME_MAX_BYTES],
    uint8_t *frame_length
)
{
    unsigned int attempts;
    for (attempts = 0u; attempts < 8u; ++attempts) {
        if (!read_frame(fd, frame, frame_length)) {
            return 0;
        }
        if (*frame_length >= 4u
            && frame[0] == 0x4eu
            && frame[1] == 0x51u
            && frame[2] == 1u) {
            return 1;
        }
    }
    return 0;
}

static int write_frame(int fd, const uint8_t *frame, uint8_t length)
{
    if (!write_exact(fd, &length, 1u)) {
        return 0;
    }
    return write_exact(fd, frame, length);
}

static int open_serial(const char *path)
{
    struct termios settings;
    struct timespec reset_pulse = { 0, 100000000 };
    struct timespec settle = { 2, 0 };
    int modem_bit = TIOCM_DTR;
    int fd = open(path, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        return -1;
    }
    if (tcgetattr(fd, &settings) != 0) {
        close(fd);
        return -1;
    }
    cfmakeraw(&settings);
    if (cfsetispeed(&settings, B115200) != 0
        || cfsetospeed(&settings, B115200) != 0) {
        close(fd);
        return -1;
    }
    settings.c_cflag |= CLOCAL | CREAD;
    settings.c_cflag &= (tcflag_t)~CSTOPB;
#ifdef CRTSCTS
    settings.c_cflag &= (tcflag_t)~CRTSCTS;
#endif
    settings.c_cc[VMIN] = 0;
    settings.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &settings) != 0) {
        close(fd);
        return -1;
    }
    (void)tcflush(fd, TCIOFLUSH);
    (void)ioctl(fd, TIOCMBIC, &modem_bit);
    (void)nanosleep(&reset_pulse, NULL);
    (void)ioctl(fd, TIOCMBIS, &modem_bit);
    (void)nanosleep(&settle, NULL);
    return fd;
}

static uint8_t *read_file_exact(const char *path, size_t expected_length)
{
    FILE *handle = fopen(path, "rb");
    uint8_t *bytes;
    int trailing;
    if (handle == NULL) {
        return NULL;
    }
    bytes = (uint8_t *)malloc(expected_length);
    if (bytes == NULL
        || fread(bytes, 1u, expected_length, handle) != expected_length) {
        free(bytes);
        fclose(handle);
        return NULL;
    }
    trailing = fgetc(handle);
    fclose(handle);
    if (trailing != EOF) {
        free(bytes);
        return NULL;
    }
    return bytes;
}

static int fill_random(uint8_t *output, size_t length)
{
    size_t offset = 0u;
    while (offset < length) {
        ssize_t received = getrandom(output + offset, length - offset, 0u);
        if (received < 0 && errno == EINTR) {
            continue;
        }
        if (received <= 0) {
            return 0;
        }
        offset += (size_t)received;
    }
    return 1;
}

static uint32_t load_latest_epoch(const char *path)
{
    FILE *handle = fopen(path, "r");
    uint32_t epoch = 0u;
    if (handle == NULL) {
        return 0u;
    }
    if (fscanf(handle, "%" SCNu32, &epoch) != 1) {
        epoch = 0u;
    }
    fclose(handle);
    return epoch;
}

static int store_latest_epoch(const char *path, uint32_t epoch)
{
    char temporary[PATH_MAX];
    FILE *handle;
    int descriptor;
    if (snprintf(
            temporary,
            sizeof(temporary),
            "%s.tmp",
            path
        ) >= (int)sizeof(temporary)) {
        return 0;
    }
    handle = fopen(temporary, "w");
    if (handle == NULL
        || fprintf(handle, "%" PRIu32 "\n", epoch) < 0
        || fflush(handle) != 0) {
        if (handle != NULL) {
            fclose(handle);
        }
        return 0;
    }
    descriptor = fileno(handle);
    if (fsync(descriptor) != 0 || fclose(handle) != 0) {
        return 0;
    }
    return rename(temporary, path) == 0;
}

static int run_enrollment(
    int fd,
    const peer_options *options,
    uint8_t algorithm_id
)
{
    uint8_t frame[NPQ_FRAME_MAX_BYTES];
    uint8_t frame_length = 0u;
    const char *signature_path;
    size_t signature_length;
    const char *algorithm_name;
    uint8_t *manifest = read_file_exact(
        options->manifest,
        PEER_MANIFEST_BYTES
    );
    uint8_t *signature = NULL;
    size_t signature_offset = 0u;
    int success = 0;
    if (algorithm_id == 1u) {
        signature_path = options->signature != NULL
            ? options->signature
            : "tests/vectors/slhdsa_signature.bin";
        signature_length = QP_SLHDSA_SIGNATURE_BYTES;
        algorithm_name = "FIPS 205 SLH-DSA-SHA2-128s";
    } else if (algorithm_id == 2u) {
        signature_path = options->signature != NULL
            ? options->signature
            : "tests/vectors/lms_w4_signature.bin";
        signature_length = NPQ_LMS_W4_SIGNATURE_BYTES;
        algorithm_name = "RFC 8554 LMS H5/W4";
    } else if (algorithm_id == 3u) {
        signature_path = options->signature != NULL
            ? options->signature
            : "tests/vectors/lms_w8_signature.bin";
        signature_length = NPQ_LMS_W8_SIGNATURE_BYTES;
        algorithm_name = "RFC 8554 LMS H5/W8";
    } else {
        fprintf(stderr, "Nano requested unknown enrollment algorithm\n");
        goto cleanup;
    }
    signature = read_file_exact(signature_path, signature_length);
    if (manifest == NULL || signature == NULL) {
        fprintf(stderr, "failed to load exact-size enrollment vectors\n");
        goto cleanup;
    }
    frame[0] = 0x4eu;
    frame[1] = 0x51u;
    frame[2] = 1u;
    frame[3] = NPQ_FRAME_ENROLL_BEGIN;
    memcpy(frame + 4u, manifest, PEER_MANIFEST_BYTES);
    if (!write_frame(fd, frame, PEER_ENROLL_FRAME_BYTES)) {
        fprintf(stderr, "failed to send enrollment manifest\n");
        goto cleanup;
    }
    printf(
        "ENROLL: %s policy received; streaming %zu bytes "
        "with verifier-driven flow control\n",
        algorithm_name,
        signature_length
    );
    for (;;) {
        size_t requested;
        if (!read_protocol_frame(fd, frame, &frame_length)) {
            fprintf(stderr, "timeout during streamed SLH-DSA verification\n");
            goto cleanup;
        }
        if (frame[3] == NPQ_FRAME_ENROLL_CHUNK_READY
            && frame_length == 6u) {
            requested = ((size_t)frame[4] << 8u) | frame[5];
            if (requested == 0u
                || requested > signature_length - signature_offset
                || !write_exact(
                    fd,
                    signature + signature_offset,
                    requested
                )) {
                fprintf(stderr, "invalid enrollment chunk request\n");
                goto cleanup;
            }
            signature_offset += requested;
        } else if (frame[3] == NPQ_FRAME_ENROLL_RESULT
            && frame_length == 5u) {
            if (frame[4] != 1u
                || signature_offset != signature_length) {
                fprintf(stderr, "Nano rejected post-quantum authorization\n");
                goto cleanup;
            }
            printf(
                "PASS: Nano verified the streamed signature without "
                "buffering it in SRAM\n"
            );
            success = 1;
            break;
        } else {
            fprintf(stderr, "unexpected enrollment frame type %u\n", frame[3]);
            goto cleanup;
        }
    }

cleanup:
    if (manifest != NULL) {
        npq_secure_zero(manifest, PEER_MANIFEST_BYTES);
    }
    if (signature != NULL) {
        npq_secure_zero(signature, signature_length);
    }
    free(manifest);
    free(signature);
    return success;
}

static int establish_session(
    int fd,
    const peer_options *options,
    const uint8_t hello[NPQ_HELLO_BYTES],
    npq_session *session
)
{
    uint8_t frame[NPQ_FRAME_MAX_BYTES];
    uint8_t challenge[NPQ_CHALLENGE_BYTES];
    uint8_t server_nonce[NPQ_NONCE_BYTES];
    uint8_t frame_length = 0u;
    uint32_t boot_epoch;
    uint32_t latest_epoch;
    int result;
    result = npq_verify_hello(hello, demo_root_key);
    if (result != NPQ_OK
        || !npq_constant_time_equal(
            hello + 4u,
            demo_device_id,
            NPQ_DEVICE_ID_BYTES
        )) {
        fprintf(stderr, "rejected unauthenticated or unknown Nano hello\n");
        return 0;
    }
    boot_epoch = npq_load_u32_be(hello + 12u);
    latest_epoch = load_latest_epoch(options->state);
    if (boot_epoch <= latest_epoch) {
        fprintf(
            stderr,
            "rejected reset/replay epoch %" PRIu32
            " (latest accepted is %" PRIu32 ")\n",
            boot_epoch,
            latest_epoch
        );
        return 0;
    }
    if (!fill_random(server_nonce, sizeof(server_nonce))
        || npq_make_challenge(
            challenge,
            demo_root_key,
            hello,
            server_nonce
        ) != NPQ_OK
        || !write_frame(fd, challenge, NPQ_CHALLENGE_BYTES)
        || npq_derive_session(
            session,
            NPQ_ROLE_SERVER,
            demo_root_key,
            hello,
            challenge
        ) != NPQ_OK) {
        fprintf(stderr, "failed to create authenticated challenge\n");
        return 0;
    }
    if (!read_protocol_frame(fd, frame, &frame_length)
        || frame_length != NPQ_FINISHED_BYTES
        || npq_verify_finished(
            frame,
            NPQ_FRAME_CLIENT_FINISHED,
            NPQ_ROLE_SERVER,
            demo_root_key,
            hello,
            challenge
        ) != NPQ_OK) {
        fprintf(stderr, "rejected client key confirmation\n");
        return 0;
    }
    if (!store_latest_epoch(options->state, boot_epoch)) {
        fprintf(stderr, "failed to persist anti-reset epoch state\n");
        return 0;
    }
    if (npq_make_finished(
            frame,
            NPQ_FRAME_SERVER_FINISHED,
            NPQ_ROLE_SERVER,
            demo_root_key,
            hello,
            challenge
        ) != NPQ_OK
        || !write_frame(fd, frame, NPQ_FINISHED_BYTES)) {
        fprintf(stderr, "failed to send server key confirmation\n");
        return 0;
    }
    printf(
        "PASS: mutual HMAC/HKDF session established at boot epoch "
        "%" PRIu32 "\n",
        boot_epoch
    );
    npq_secure_zero(challenge, sizeof(challenge));
    npq_secure_zero(server_nonce, sizeof(server_nonce));
    npq_secure_zero(frame, sizeof(frame));
    return 1;
}

static int receive_plaintext(
    int fd,
    npq_session *session,
    uint8_t plaintext[NPQ_DATA_MAX_BYTES],
    uint8_t *plaintext_length
)
{
    uint8_t frame[NPQ_FRAME_MAX_BYTES];
    uint8_t frame_length = 0u;
    return read_protocol_frame(fd, frame, &frame_length)
        && npq_open(
            session,
            frame,
            frame_length,
            plaintext,
            plaintext_length
        ) == NPQ_OK;
}

static int require_status(
    int fd,
    npq_session *session,
    uint8_t expected_accepted,
    const char *label
)
{
    uint8_t plaintext[NPQ_DATA_MAX_BYTES];
    uint8_t plaintext_length = 0u;
    if (!receive_plaintext(
            fd,
            session,
            plaintext,
            &plaintext_length
        )
        || plaintext_length != 3u
        || plaintext[0] != 0x02u
        || plaintext[1] != expected_accepted) {
        fprintf(stderr, "failed: %s\n", label);
        return 0;
    }
    printf(
        "PASS: %s (LED=%s)\n",
        label,
        plaintext[2] != 0u ? "on" : "off"
    );
    npq_secure_zero(plaintext, sizeof(plaintext));
    return 1;
}

static int receive_sensor(
    int fd,
    npq_session *session
)
{
    uint8_t plaintext[NPQ_DATA_MAX_BYTES];
    uint8_t plaintext_length = 0u;
    uint16_t adc;
    if (!receive_plaintext(
            fd,
            session,
            plaintext,
            &plaintext_length
        )
        || plaintext_length != 7u
        || plaintext[0] != 0x01u) {
        fprintf(stderr, "failed to receive authenticated sensor record\n");
        return 0;
    }
    adc = (uint16_t)(((uint16_t)plaintext[1] << 8u) | plaintext[2]);
    printf(
        "PASS: Ascon-AEAD128 sensor record ADC=%u, epoch=%" PRIu32 "\n",
        adc,
        npq_load_u32_be(plaintext + 3u)
    );
    npq_secure_zero(plaintext, sizeof(plaintext));
    return 1;
}

static int run_live_demo(
    int fd,
    npq_session *session,
    const peer_options *options
)
{
    uint8_t command[2] = { 0x10u, options->led };
    uint8_t command_frame[NPQ_FRAME_MAX_BYTES];
    uint8_t command_length = 0u;
    uint8_t tampered[NPQ_FRAME_MAX_BYTES];
    if (!receive_sensor(fd, session)) {
        return 0;
    }
    if (npq_seal(
            session,
            command,
            sizeof(command),
            command_frame,
            &command_length
        ) != NPQ_OK) {
        return 0;
    }
    if (options->tamper_demo != 0u) {
        memcpy(tampered, command_frame, command_length);
        tampered[command_length - 1u] ^= 1u;
        if (!write_frame(fd, tampered, command_length)
            || !require_status(
                fd,
                session,
                0u,
                "tampered command rejected without ratchet advance"
            )) {
            return 0;
        }
        if (!receive_sensor(fd, session)) {
            return 0;
        }
    }
    if (!write_frame(fd, command_frame, command_length)
        || !require_status(
            fd,
            session,
            1u,
            options->led != 0u
                ? "valid encrypted LED-on command accepted"
                : "valid encrypted LED-off command accepted"
        )) {
        return 0;
    }
    if (options->tamper_demo != 0u) {
        if (!receive_sensor(fd, session)
            || !write_frame(fd, command_frame, command_length)
            || !require_status(
                fd,
                session,
                0u,
                "replayed command rejected"
            )) {
            return 0;
        }
    }
    npq_secure_zero(command_frame, sizeof(command_frame));
    npq_secure_zero(tampered, sizeof(tampered));
    return 1;
}

int main(int argc, char **argv)
{
    peer_options options;
    npq_session session;
    uint8_t first[NPQ_FRAME_MAX_BYTES];
    uint8_t first_length = 0u;
    int fd;
    int success = 0;
    memset(&session, 0, sizeof(session));
    if (!parse_options(argc, argv, &options)) {
        usage(argv[0]);
        return 2;
    }
    fd = open_serial(options.port);
    if (fd < 0) {
        fprintf(stderr, "cannot open %s: %s\n", options.port, strerror(errno));
        return 2;
    }
    if (!read_protocol_frame(fd, first, &first_length)) {
        fprintf(stderr, "Nano did not produce a protocol frame\n");
        goto cleanup;
    }
    if (first[3] == NPQ_FRAME_ENROLL_REQUIRED) {
        if (first_length != 5u
            || !run_enrollment(fd, &options, first[4])
            || !read_protocol_frame(fd, first, &first_length)) {
            goto cleanup;
        }
    }
    if (first[3] != NPQ_FRAME_HELLO
        || first_length != NPQ_HELLO_BYTES
        || !establish_session(fd, &options, first, &session)
        || !run_live_demo(fd, &session, &options)) {
        goto cleanup;
    }
    printf(
        "RESULT: PASS — post-quantum authorization, reset-safe session, "
        "encrypted traffic, tamper rejection, and replay rejection\n"
    );
    success = 1;

cleanup:
    npq_close(&session);
    npq_secure_zero(first, sizeof(first));
    close(fd);
    return success ? 0 : 1;
}
