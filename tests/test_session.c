#include "c0pqlink/c0pqlink.h"

#include "session/internal.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t bytes[2][C0PQ_MIGRATION_RECORD_BYTES];
    int present[2];
    int tear_next_write;
    int read_failure;
} journal_storage;

static int expect_true(const char *name, int condition)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        return 1;
    }
    return 0;
}

static uint8_t dummy_public_key_read(void *context, uint16_t offset)
{
    (void)context;
    return (uint8_t)offset;
}

static int dummy_random(void *context, uint8_t *output, size_t length)
{
    (void)context;
    memset(output, 0x55, length);
    return 0;
}

static int dummy_send(
    void *context,
    const uint8_t *frame,
    size_t frame_length
)
{
    (void)context;
    (void)frame;
    (void)frame_length;
    return 0;
}

static int dummy_receive(
    void *context,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *frame_length,
    uint32_t timeout_ms
)
{
    (void)context;
    (void)frame;
    (void)frame_capacity;
    (void)frame_length;
    (void)timeout_ms;
    return -1;
}

static void fill_bytes(uint8_t *output, size_t length, uint8_t start)
{
    size_t i;
    for (i = 0u; i < length; ++i) {
        output[i] = (uint8_t)(start + i);
    }
}

static int journal_read(
    void *context,
    uint8_t slot,
    uint8_t output[C0PQ_MIGRATION_RECORD_BYTES]
)
{
    journal_storage *storage = (journal_storage *)context;
    if (storage->read_failure != 0) {
        return -1;
    }
    if (slot > 1u || storage->present[slot] == 0) {
        return C0PQ_JOURNAL_READ_EMPTY;
    }
    memcpy(output, storage->bytes[slot], C0PQ_MIGRATION_RECORD_BYTES);
    return C0PQ_JOURNAL_READ_OK;
}

static int journal_write(
    void *context,
    uint8_t slot,
    const uint8_t record[C0PQ_MIGRATION_RECORD_BYTES]
)
{
    journal_storage *storage = (journal_storage *)context;
    size_t length = C0PQ_MIGRATION_RECORD_BYTES;
    if (slot > 1u) {
        return -1;
    }
    if (storage->tear_next_write != 0) {
        length /= 2u;
        storage->tear_next_write = 0;
    }
    memset(storage->bytes[slot], 0xa5, C0PQ_MIGRATION_RECORD_BYTES);
    memcpy(storage->bytes[slot], record, length);
    storage->present[slot] = 1;
    return 0;
}

static int test_preflight(void)
{
    c0pq_hello hello;
    c0pq_hello decoded_hello;
    c0pq_challenge challenge;
    c0pq_challenge decoded_challenge;
    uint8_t psk[C0PQ_PSK_BYTES];
    uint8_t hello_frame[C0PQ_HELLO_FRAME_BYTES];
    uint8_t challenge_frame[C0PQ_CHALLENGE_FRAME_BYTES];
    uint32_t session_id = 0u;
    int failures = 0;
    fill_bytes(psk, sizeof(psk), 0x10u);
    memset(&hello, 0, sizeof(hello));
    hello.suite = C0PQ_SUITE_MLKEM512_HKDFSHA256_ASCON128;
    fill_bytes(hello.device_id, sizeof(hello.device_id), 1u);
    hello.epoch = 7u;
    fill_bytes(hello.device_nonce, sizeof(hello.device_nonce), 0x20u);
    failures += expect_true(
        "encode authenticated Hello",
        c0pq_encode_hello(
            hello_frame,
            UINT32_C(0x11223344),
            &hello,
            psk
        ) == C0PQLINK_OK
    );
    failures += expect_true(
        "decode authenticated Hello",
        c0pq_decode_verify_hello(
            &decoded_hello,
            &session_id,
            hello_frame,
            sizeof(hello_frame),
            psk
        ) == C0PQLINK_OK
        && session_id == UINT32_C(0x11223344)
        && decoded_hello.epoch == hello.epoch
        && c0pqlink_ct_equal(
            decoded_hello.device_nonce,
            hello.device_nonce,
            sizeof(hello.device_nonce)
        )
    );
    hello_frame[30] ^= 1u;
    failures += expect_true(
        "reject forged Hello",
        c0pq_decode_verify_hello(
            &decoded_hello,
            &session_id,
            hello_frame,
            sizeof(hello_frame),
            psk
        ) == C0PQLINK_ERR_AUTH
    );
    hello_frame[30] ^= 1u;
    memset(&challenge, 0, sizeof(challenge));
    challenge.suite = C0PQ_SUITE_MLKEM512_HKDFSHA256_ASCON128;
    challenge.epoch = hello.epoch;
    fill_bytes(challenge.key_id, sizeof(challenge.key_id), 0x40u);
    memcpy(
        challenge.device_nonce,
        hello.device_nonce,
        sizeof(challenge.device_nonce)
    );
    fill_bytes(challenge.server_nonce, sizeof(challenge.server_nonce), 0x70u);
    failures += expect_true(
        "encode authenticated Challenge",
        c0pq_encode_challenge(
            challenge_frame,
            session_id,
            &challenge,
            psk
        ) == C0PQLINK_OK
    );
    failures += expect_true(
        "decode authenticated Challenge",
        c0pq_decode_verify_challenge(
            &decoded_challenge,
            session_id,
            challenge_frame,
            sizeof(challenge_frame),
            psk
        ) == C0PQLINK_OK
        && decoded_challenge.epoch == challenge.epoch
        && c0pqlink_ct_equal(
            decoded_challenge.key_id,
            challenge.key_id,
            sizeof(challenge.key_id)
        )
    );
    challenge_frame[67] ^= 1u;
    failures += expect_true(
        "reject forged Challenge",
        c0pq_decode_verify_challenge(
            &decoded_challenge,
            session_id,
            challenge_frame,
            sizeof(challenge_frame),
            psk
        ) == C0PQLINK_ERR_AUTH
    );
    return failures;
}

static int test_client_configuration(void)
{
    c0pq_client client;
    c0pq_client_config config;
    int failures = 0;
    memset(&config, 0, sizeof(config));
    config.read_public_key = dummy_public_key_read;
    config.random_bytes = dummy_random;
    config.send_frame = dummy_send;
    config.receive_frame = dummy_receive;
    config.mode = C0PQ_PQ_BOOTSTRAP_RATCHET;
    failures += expect_true(
        "reject all-zero provisioning PSK",
        c0pq_client_init(&client, &config) == C0PQLINK_ERR_ARGUMENT
    );
    memset(config.psk, 0x11, sizeof(config.psk));
    failures += expect_true(
        "reject all-zero public-key ID",
        c0pq_client_init(&client, &config) == C0PQLINK_ERR_ARGUMENT
    );
    memset(config.public_key_id, 0x22, sizeof(config.public_key_id));
    failures += expect_true(
        "accept non-placeholder client provisioning",
        c0pq_client_init(&client, &config) == C0PQLINK_OK
    );
    return failures;
}

static int test_records(void)
{
    static const uint8_t outbound[] = "soil=41";
    static const uint8_t inbound[] = "accepted";
    c0pq_client client;
    c0pq_client tampered_client;
    c0pq_client boundary_client;
    c0pq_frame_header header;
    uint8_t frame[C0PQ_FRAME_MAX_BYTES];
    uint8_t server_frame[C0PQ_FRAME_MAX_BYTES];
    uint8_t plaintext[C0PQ_RECORD_PLAINTEXT_MAX];
    uint8_t message_key[16];
    uint8_t expected_chain[32];
    uint8_t original_client_chain[32];
    uint8_t original_server_chain[32];
    uint8_t nonce[16];
    uint8_t maximum_plaintext[C0PQ_RECORD_PLAINTEXT_MAX];
    size_t frame_length = 0u;
    size_t plaintext_length = 0u;
    const size_t inbound_length = sizeof(inbound) - 1u;
    const size_t server_length =
        C0PQ_FRAME_HEADER_BYTES + 8u + inbound_length + 16u;
    int failures = 0;
    int result;
    memset(&boundary_client, 0, sizeof(boundary_client));
    boundary_client.state = C0PQ_CLIENT_ESTABLISHED;
    boundary_client.session_id = UINT32_C(0x01020304);
    boundary_client.config.mode = C0PQ_FULL_PQ_EACH_SESSION;
    fill_bytes(boundary_client.send_secret, 16u, 0x81u);
    fill_bytes(boundary_client.send_nonce_base, 16u, 0xa1u);
    fill_bytes(maximum_plaintext, sizeof(maximum_plaintext), 0xc1u);
    failures += expect_true(
        "reject oversized application record without state advance",
        c0pq_client_seal_record(
            &boundary_client,
            maximum_plaintext,
            sizeof(maximum_plaintext) + 1u,
            frame,
            sizeof(frame),
            &frame_length
        ) == C0PQLINK_ERR_CAPACITY
        && boundary_client.send_sequence == 0u
    );
    failures += expect_true(
        "seal maximum application record",
        c0pq_client_seal_record(
            &boundary_client,
            maximum_plaintext,
            sizeof(maximum_plaintext),
            frame,
            sizeof(frame),
            &frame_length
        ) == C0PQLINK_OK
        && frame_length == 82u
        && boundary_client.send_sequence == 1u
    );
    memset(&client, 0, sizeof(client));
    client.state = C0PQ_CLIENT_ESTABLISHED;
    client.session_id = UINT32_C(0xa1b2c3d4);
    client.config.mode = C0PQ_PQ_BOOTSTRAP_RATCHET;
    fill_bytes(client.send_secret, 32u, 0x11u);
    fill_bytes(client.receive_secret, 32u, 0x31u);
    fill_bytes(client.send_nonce_base, 16u, 0x51u);
    fill_bytes(client.receive_nonce_base, 16u, 0x71u);
    memcpy(
        original_client_chain,
        client.send_secret,
        sizeof(original_client_chain)
    );
    memcpy(
        original_server_chain,
        client.receive_secret,
        sizeof(original_server_chain)
    );
    result = c0pq_client_seal_record(
        &client,
        outbound,
        sizeof(outbound) - 1u,
        frame,
        sizeof(frame),
        &frame_length
    );
    failures += expect_true(
        "seal ratcheted client record",
        result == C0PQLINK_OK
        && c0pq_decode_header(&header, frame, frame_length) == C0PQLINK_OK
        && header.type == C0PQ_FRAME_DATA
        && header.flags == 0u
        && client.send_sequence == 1u
    );
    c0pq_ratchet_step(
        original_client_chain,
        message_key,
        expected_chain
    );
    c0pq_make_record_nonce(nonce, client.send_nonce_base, 0u);
    failures += expect_true(
        "client record independently opens",
        c0_ascon_aead128_decrypt(
            plaintext,
            message_key,
            nonce,
            frame,
            18u,
            frame + 18u,
            sizeof(outbound) - 1u,
            frame + 18u + sizeof(outbound) - 1u
        ) == C0PQLINK_OK
        && c0pqlink_ct_equal(
            plaintext,
            outbound,
            sizeof(outbound) - 1u
        )
        && c0pqlink_ct_equal(
            client.send_secret,
            expected_chain,
            sizeof(expected_chain)
        )
    );
    (void)c0pq_encode_header(
        server_frame,
        C0PQ_FRAME_DATA,
        1u,
        (uint16_t)(8u + inbound_length + 16u),
        client.session_id
    );
    c0pq_store64_be(server_frame + 10u, 0u);
    c0pq_ratchet_step(
        original_server_chain,
        message_key,
        expected_chain
    );
    c0pq_make_record_nonce(nonce, client.receive_nonce_base, 0u);
    c0_ascon_aead128_encrypt(
        server_frame + 18u,
        server_frame + 18u + inbound_length,
        message_key,
        nonce,
        server_frame,
        18u,
        inbound,
        inbound_length
    );
    memcpy(&tampered_client, &client, sizeof(client));
    server_frame[server_length - 1u] ^= 1u;
    memset(plaintext, 0xa5, sizeof(plaintext));
    result = c0pq_client_open_record(
        &tampered_client,
        server_frame,
        server_length,
        plaintext,
        sizeof(plaintext),
        &plaintext_length
    );
    failures += expect_true(
        "failed record is transactional",
        result == C0PQLINK_ERR_AUTH
        && tampered_client.receive_sequence == 0u
        && c0pqlink_ct_equal(
            tampered_client.receive_secret,
            original_server_chain,
            sizeof(original_server_chain)
        )
        && plaintext_length == 0u
    );
    server_frame[server_length - 1u] ^= 1u;
    result = c0pq_client_open_record(
        &client,
        server_frame,
        server_length,
        plaintext,
        sizeof(plaintext),
        &plaintext_length
    );
    failures += expect_true(
        "open ratcheted server record",
        result == C0PQLINK_OK
        && plaintext_length == inbound_length
        && c0pqlink_ct_equal(plaintext, inbound, inbound_length)
        && client.receive_sequence == 1u
        && c0pqlink_ct_equal(
            client.receive_secret,
            expected_chain,
            sizeof(expected_chain)
        )
    );
    failures += expect_true(
        "reject replayed server record",
        c0pq_client_open_record(
            &client,
            server_frame,
            server_length,
            plaintext,
            sizeof(plaintext),
            &plaintext_length
        ) == C0PQLINK_ERR_REPLAY
    );
    return failures;
}

static int test_migration(void)
{
    journal_storage storage;
    journal_storage failed_storage;
    journal_storage corrupted_storage;
    c0pq_migration_value value;
    c0pq_migration_value loaded;
    uint8_t key[32];
    uint8_t slot = 0u;
    int failures = 0;
    memset(&storage, 0, sizeof(storage));
    memset(&failed_storage, 0, sizeof(failed_storage));
    failed_storage.read_failure = 1;
    fill_bytes(key, sizeof(key), 0x90u);
    memset(&value, 0, sizeof(value));
    value.state = C0PQ_MIGRATION_PSK_PLUS_PQ;
    value.active_key_slot = 0u;
    value.epoch = 4u;
    fill_bytes(value.key_id, sizeof(value.key_id), 0x20u);
    failures += expect_true(
        "migration initialization fails closed on storage I/O",
        c0pq_migration_initialize(
            &value,
            journal_read,
            journal_write,
            &failed_storage,
            key
        ) == C0PQLINK_ERR_IO
    );
    failures += expect_true(
        "initial migration journal provisioning",
        c0pq_migration_initialize(
            &value,
            journal_read,
            journal_write,
            &storage,
            key
        ) == C0PQLINK_OK
    );
    failures += expect_true(
        "migration journal cannot be reinitialized",
        c0pq_migration_initialize(
            &value,
            journal_read,
            journal_write,
            &storage,
            key
        ) == C0PQLINK_ERR_STATE
    );
    memcpy(&corrupted_storage, &storage, sizeof(corrupted_storage));
    corrupted_storage.bytes[0][51] ^= 1u;
    failures += expect_true(
        "corrupt journal cannot be treated as first provisioning",
        c0pq_migration_commit(
            &value,
            journal_read,
            journal_write,
            &corrupted_storage,
            key
        ) == C0PQLINK_ERR_STATE
    );
    failures += expect_true(
        "load migration journal",
        c0pq_migration_load(
            &loaded,
            &slot,
            journal_read,
            &storage,
            key
        ) == C0PQLINK_OK
        && loaded.generation == 1u
        && loaded.state == C0PQ_MIGRATION_PSK_PLUS_PQ
    );
    value.state = C0PQ_MIGRATION_PQ_REQUIRED_WITH_PSK_AUTH;
    value.active_key_slot = 1u;
    value.epoch = 5u;
    fill_bytes(value.key_id, sizeof(value.key_id), 0x40u);
    failures += expect_true(
        "one-way migration upgrade",
        c0pq_migration_commit(
            &value,
            journal_read,
            journal_write,
            &storage,
            key
        ) == C0PQLINK_OK
        && c0pq_migration_load(
            &loaded,
            &slot,
            journal_read,
            &storage,
            key
        ) == C0PQLINK_OK
        && loaded.generation == 2u
        && loaded.state == C0PQ_MIGRATION_PQ_REQUIRED_WITH_PSK_AUTH
        && loaded.epoch == 5u
    );
    value.state = C0PQ_MIGRATION_PSK_PLUS_PQ;
    failures += expect_true(
        "migration downgrade rejected",
        c0pq_migration_commit(
            &value,
            journal_read,
            journal_write,
            &storage,
            key
        ) == C0PQLINK_ERR_STATE
    );
    value.state = C0PQ_MIGRATION_PQ_REQUIRED_WITH_PSK_AUTH;
    value.epoch = 6u;
    storage.tear_next_write = 1;
    failures += expect_true(
        "torn journal write detected",
        c0pq_migration_commit(
            &value,
            journal_read,
            journal_write,
            &storage,
            key
        ) == C0PQLINK_ERR_IO
    );
    failures += expect_true(
        "old journal survives torn write",
        c0pq_migration_load(
            &loaded,
            &slot,
            journal_read,
            &storage,
            key
        ) == C0PQLINK_OK
        && loaded.generation == 2u
        && loaded.epoch == 5u
    );
    return failures;
}

int test_session(void)
{
    int failures = 0;
    failures += test_client_configuration();
    failures += test_preflight();
    failures += test_records();
    failures += test_migration();
    return failures;
}
