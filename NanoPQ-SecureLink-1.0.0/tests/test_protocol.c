#include "ascon_aead128.h"
#include "protocol.h"
#include "sha256.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void expect(int condition, const char *name)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        ++failures;
    }
}

static void test_primitives(void)
{
    static const uint8_t sha256_abc[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    static const uint8_t hmac_expected[32] = {
        0x19, 0x8a, 0x60, 0x7e, 0xb4, 0x4b, 0xfb, 0xc6,
        0x99, 0x03, 0xa0, 0xf1, 0xcf, 0x2b, 0xbd, 0xc5,
        0xba, 0x0a, 0xa3, 0xf3, 0xd9, 0xae, 0x3c, 0x1c,
        0x7a, 0x3b, 0x16, 0x96, 0xa0, 0xb6, 0x8c, 0xf7
    };
    static const uint8_t ascon_expected[31] = {
        0xb3, 0xb0, 0x5f, 0x0a, 0x08, 0xc5, 0x76, 0x97,
        0x49, 0xa3, 0xb5, 0x65, 0x8b, 0xcb, 0x1d,
        0x9c, 0x4c, 0x29, 0x70, 0xaa, 0x15, 0x4b, 0x85,
        0x47, 0xc7, 0xd0, 0xfd, 0x2a, 0xd4, 0x55, 0xa0
    };
    uint8_t output[32];
    uint8_t key[16];
    uint8_t nonce[16];
    uint8_t plaintext[15];
    uint8_t associated_data[18];
    uint8_t ciphertext[15];
    uint8_t decrypted[15];
    uint8_t tag[16];
    uint8_t hmac_key[32];
    unsigned int index;

    qp_sha256((const uint8_t *)"abc", 3u, output);
    expect(
        npq_constant_time_equal(output, sha256_abc, sizeof(output)),
        "SHA-256 abc"
    );

    memset(hmac_key, 0x0bu, sizeof(hmac_key));
    qp_hmac_sha256_32(
        hmac_key,
        (const uint8_t *)"Hi There",
        8u,
        output
    );
    expect(
        npq_constant_time_equal(output, hmac_expected, sizeof(output)),
        "HMAC-SHA-256 fixed 32-byte key"
    );

    for (index = 0u; index < sizeof(associated_data); ++index) {
        if (index < sizeof(key)) {
            key[index] = (uint8_t)index;
            nonce[index] = (uint8_t)(0x10u + index);
        }
        if (index < sizeof(plaintext)) {
            plaintext[index] = (uint8_t)(0x20u + index);
        }
        associated_data[index] = (uint8_t)(0x30u + index);
    }
    npq_ascon_aead128_encrypt(
        ciphertext,
        tag,
        key,
        nonce,
        associated_data,
        sizeof(associated_data),
        plaintext,
        sizeof(plaintext)
    );
    expect(
        npq_constant_time_equal(
            ciphertext,
            ascon_expected,
            sizeof(ciphertext)
        ),
        "Ascon-AEAD128 ciphertext vector"
    );
    expect(
        npq_constant_time_equal(
            tag,
            ascon_expected + sizeof(ciphertext),
            sizeof(tag)
        ),
        "Ascon-AEAD128 tag vector"
    );
    expect(
        npq_ascon_aead128_decrypt(
            decrypted,
            key,
            nonce,
            associated_data,
            sizeof(associated_data),
            ciphertext,
            sizeof(ciphertext),
            tag
        ) == NPQ_OK
            && npq_constant_time_equal(
                decrypted,
                plaintext,
                sizeof(plaintext)
            ),
        "Ascon-AEAD128 round trip"
    );
    tag[0] ^= 1u;
    memset(decrypted, 0xa5, sizeof(decrypted));
    expect(
        npq_ascon_aead128_decrypt(
            decrypted,
            key,
            nonce,
            associated_data,
            sizeof(associated_data),
            ciphertext,
            sizeof(ciphertext),
            tag
        ) == NPQ_ERR_AUTH,
        "Ascon-AEAD128 tamper rejection"
    );
    for (index = 0u; index < sizeof(decrypted); ++index) {
        expect(decrypted[index] == 0u, "Ascon failure zeroization");
    }
}

static void establish(
    npq_session *device,
    npq_session *server,
    const uint8_t root_key[32],
    const uint8_t device_id[8],
    uint32_t epoch,
    const uint8_t server_nonce[16],
    uint8_t hello[NPQ_HELLO_BYTES],
    uint8_t challenge[NPQ_CHALLENGE_BYTES]
)
{
    uint8_t client_finished[NPQ_FINISHED_BYTES];
    uint8_t server_finished[NPQ_FINISHED_BYTES];
    expect(
        npq_make_hello(hello, root_key, device_id, epoch) == NPQ_OK,
        "make hello"
    );
    expect(
        npq_verify_hello(hello, root_key) == NPQ_OK,
        "verify hello"
    );
    expect(
        npq_make_challenge(
            challenge,
            root_key,
            hello,
            server_nonce
        ) == NPQ_OK,
        "make challenge"
    );
    expect(
        npq_verify_challenge(challenge, root_key, hello) == NPQ_OK,
        "verify challenge"
    );
    expect(
        npq_derive_session(
            device,
            NPQ_ROLE_DEVICE,
            root_key,
            hello,
            challenge
        ) == NPQ_OK,
        "derive device session"
    );
    expect(
        npq_derive_session(
            server,
            NPQ_ROLE_SERVER,
            root_key,
            hello,
            challenge
        ) == NPQ_OK,
        "derive server session"
    );
    expect(
        npq_make_finished(
            client_finished,
            NPQ_FRAME_CLIENT_FINISHED,
            NPQ_ROLE_DEVICE,
            root_key,
            hello,
            challenge
        ) == NPQ_OK,
        "make client finished"
    );
    expect(
        npq_verify_finished(
            client_finished,
            NPQ_FRAME_CLIENT_FINISHED,
            NPQ_ROLE_SERVER,
            root_key,
            hello,
            challenge
        ) == NPQ_OK,
        "verify client finished"
    );
    expect(
        npq_make_finished(
            server_finished,
            NPQ_FRAME_SERVER_FINISHED,
            NPQ_ROLE_SERVER,
            root_key,
            hello,
            challenge
        ) == NPQ_OK,
        "make server finished"
    );
    expect(
        npq_verify_finished(
            server_finished,
            NPQ_FRAME_SERVER_FINISHED,
            NPQ_ROLE_DEVICE,
            root_key,
            hello,
            challenge
        ) == NPQ_OK,
        "verify server finished"
    );
}

static void test_session(void)
{
    static const uint8_t root_key[32] = {
        0x1b, 0x47, 0x6c, 0xe0, 0x0b, 0x45, 0x87, 0x91,
        0x7e, 0xc4, 0xee, 0x14, 0xa2, 0x5e, 0x1a, 0xf6,
        0xb5, 0x3a, 0xb3, 0x39, 0x6d, 0x6c, 0x28, 0x1f,
        0x5a, 0x7a, 0x84, 0xc7, 0x16, 0x67, 0x33, 0x42
    };
    static const uint8_t device_id[8] = {
        0x03, 0xaf, 0xe9, 0xd9, 0x89, 0x40, 0xf1, 0xd2
    };
    static const uint8_t server_nonce[16] = {
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f
    };
    static const uint8_t sensor_record[] = {
        0x01, 0x02, 0x7b, 0x00, 0x00, 0x00, 0x2a
    };
    static const uint8_t command[] = { 0x10, 0x01 };
    npq_session device;
    npq_session server;
    npq_session server_before_tamper;
    npq_session reset_device;
    npq_session reset_server;
    uint8_t hello[NPQ_HELLO_BYTES];
    uint8_t challenge[NPQ_CHALLENGE_BYTES];
    uint8_t reset_hello[NPQ_HELLO_BYTES];
    uint8_t reset_challenge[NPQ_CHALLENGE_BYTES];
    uint8_t frame[NPQ_FRAME_MAX_BYTES];
    uint8_t replay[NPQ_FRAME_MAX_BYTES];
    uint8_t plaintext[NPQ_DATA_MAX_BYTES];
    uint8_t frame_length = 0u;
    uint8_t plaintext_length = 0u;

    establish(
        &device,
        &server,
        root_key,
        device_id,
        42u,
        server_nonce,
        hello,
        challenge
    );
    expect(
        device.session_id == server.session_id,
        "matching session id"
    );
    expect(
        npq_constant_time_equal(
            device.send_chain,
            server.receive_chain,
            NPQ_CHAIN_KEY_BYTES
        ),
        "matching client chain"
    );
    expect(
        npq_seal(
            &device,
            sensor_record,
            sizeof(sensor_record),
            frame,
            &frame_length
        ) == NPQ_OK,
        "seal sensor record"
    );
    memcpy(replay, frame, frame_length);
    expect(
        npq_open(
            &server,
            frame,
            frame_length,
            plaintext,
            &plaintext_length
        ) == NPQ_OK
            && plaintext_length == sizeof(sensor_record)
            && npq_constant_time_equal(
                plaintext,
                sensor_record,
                sizeof(sensor_record)
            ),
        "open sensor record"
    );
    expect(
        npq_open(
            &server,
            replay,
            frame_length,
            plaintext,
            &plaintext_length
        ) == NPQ_ERR_REPLAY,
        "replay rejected"
    );

    expect(
        npq_seal(
            &server,
            command,
            sizeof(command),
            frame,
            &frame_length
        ) == NPQ_OK,
        "seal server command"
    );
    memcpy(&server_before_tamper, &device, sizeof(device));
    frame[13] ^= 0x80u;
    expect(
        npq_open(
            &device,
            frame,
            frame_length,
            plaintext,
            &plaintext_length
        ) == NPQ_ERR_AUTH,
        "ciphertext tamper rejected"
    );
    expect(
        device.receive_sequence
            == server_before_tamper.receive_sequence
            && npq_constant_time_equal(
                device.receive_chain,
                server_before_tamper.receive_chain,
                NPQ_CHAIN_KEY_BYTES
            ),
        "tamper does not advance ratchet"
    );
    frame[13] ^= 0x80u;
    expect(
        npq_open(
            &device,
            frame,
            frame_length,
            plaintext,
            &plaintext_length
        ) == NPQ_OK
            && plaintext_length == sizeof(command)
            && npq_constant_time_equal(
                plaintext,
                command,
                sizeof(command)
            ),
        "valid command accepted after tamper"
    );

    establish(
        &reset_device,
        &reset_server,
        root_key,
        device_id,
        43u,
        server_nonce,
        reset_hello,
        reset_challenge
    );
    expect(
        !npq_constant_time_equal(
            device.send_chain,
            reset_device.send_chain,
            NPQ_CHAIN_KEY_BYTES
        ),
        "new boot epoch derives fresh chain"
    );
    expect(
        npq_verify_challenge(
            challenge,
            root_key,
            reset_hello
        ) != NPQ_OK,
        "old challenge rejected after reset"
    );
    npq_close(&device);
    npq_close(&server);
    npq_close(&reset_device);
    npq_close(&reset_server);
}

int main(void)
{
    test_primitives();
    test_session();
    if (failures != 0) {
        fprintf(stderr, "%d NanoPQ test(s) failed\n", failures);
        return 1;
    }
    puts(
        "PASS: primitives, handshake, key confirmation, encrypted sensor "
        "traffic, tamper, replay, ratchet, and reset freshness"
    );
    return 0;
}
