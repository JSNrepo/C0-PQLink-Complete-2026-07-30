#include "c0pqlink/migration.h"

#include "session/internal.h"

#include <string.h>

#define MIGRATION_CORE_BYTES 36u

static void encode_record(
    uint8_t output[C0PQ_MIGRATION_RECORD_BYTES],
    const c0pq_migration_value *value,
    const uint8_t journal_key[32]
)
{
    output[0] = (uint8_t)'C';
    output[1] = (uint8_t)'0';
    output[2] = (uint8_t)'M';
    output[3] = (uint8_t)'J';
    output[4] = 1u;
    output[5] = (uint8_t)value->state;
    output[6] = value->active_key_slot;
    output[7] = 0u;
    c0pq_store32_be(output + 8u, value->generation);
    c0pq_store64_be(output + 12u, value->epoch);
    memcpy(output + 20u, value->key_id, C0PQ_KEY_ID_BYTES);
    c0pq_auth_tag(
        output + MIGRATION_CORE_BYTES,
        journal_key,
        "C0PQ/1 migration journal",
        output,
        MIGRATION_CORE_BYTES
    );
}

static int decode_record(
    c0pq_migration_value *value,
    const uint8_t input[C0PQ_MIGRATION_RECORD_BYTES],
    const uint8_t journal_key[32]
)
{
    int result;
    if (input[0] != (uint8_t)'C' || input[1] != (uint8_t)'0'
        || input[2] != (uint8_t)'M' || input[3] != (uint8_t)'J'
        || input[4] != 1u || input[5] > C0PQ_MIGRATION_PQ_REQUIRED_WITH_PSK_AUTH
        || input[6] > 1u || input[7] != 0u) {
        return C0PQLINK_ERR_STATE;
    }
    result = c0pq_verify_auth_tag(
        input + MIGRATION_CORE_BYTES,
        journal_key,
        "C0PQ/1 migration journal",
        input,
        MIGRATION_CORE_BYTES
    );
    if (result != C0PQLINK_OK) {
        return result;
    }
    value->state = (c0pq_migration_state)input[5];
    value->active_key_slot = input[6];
    value->generation = c0pq_load32_be(input + 8u);
    value->epoch = c0pq_load64_be(input + 12u);
    memcpy(value->key_id, input + 20u, C0PQ_KEY_ID_BYTES);
    if (value->generation == 0u) {
        c0pqlink_secure_zero(value, sizeof(*value));
        return C0PQLINK_ERR_STATE;
    }
    return C0PQLINK_OK;
}

int c0pq_migration_load(
    c0pq_migration_value *value,
    uint8_t *record_slot,
    c0pq_journal_read_fn read_record,
    void *storage_context,
    const uint8_t journal_key[32]
)
{
    uint8_t raw[2][C0PQ_MIGRATION_RECORD_BYTES];
    c0pq_migration_value candidate[2];
    int valid[2] = { 0, 0 };
    int io_failure = 0;
    unsigned int slot;
    if (value == NULL || record_slot == NULL || read_record == NULL
        || journal_key == NULL) {
        return C0PQLINK_ERR_ARGUMENT;
    }
    memset(candidate, 0, sizeof(candidate));
    for (slot = 0u; slot < 2u; ++slot) {
        const int read_result = read_record(
            storage_context,
            (uint8_t)slot,
            raw[slot]
        );
        if (read_result == C0PQ_JOURNAL_READ_OK
            && decode_record(
                &candidate[slot],
                raw[slot],
                journal_key
            ) == C0PQLINK_OK) {
            valid[slot] = 1;
        } else if (read_result < C0PQ_JOURNAL_READ_OK) {
            io_failure = 1;
        }
    }
    c0pqlink_secure_zero(raw, sizeof(raw));
    if (valid[0] == 0 && valid[1] == 0) {
        c0pqlink_secure_zero(candidate, sizeof(candidate));
        return io_failure != 0 ? C0PQLINK_ERR_IO : C0PQLINK_ERR_STATE;
    }
    if (valid[1] != 0
        && (valid[0] == 0
            || candidate[1].generation > candidate[0].generation)) {
        memcpy(value, &candidate[1], sizeof(*value));
        *record_slot = 1u;
    } else {
        memcpy(value, &candidate[0], sizeof(*value));
        *record_slot = 0u;
    }
    c0pqlink_secure_zero(candidate, sizeof(candidate));
    return C0PQLINK_OK;
}

int c0pq_migration_transition_allowed(
    c0pq_migration_state current,
    c0pq_migration_state next
)
{
    if (current < C0PQ_MIGRATION_LEGACY_PSK
        || current > C0PQ_MIGRATION_PQ_REQUIRED_WITH_PSK_AUTH
        || next < C0PQ_MIGRATION_LEGACY_PSK
        || next > C0PQ_MIGRATION_PQ_REQUIRED_WITH_PSK_AUTH) {
        return 0;
    }
    return next >= current ? 1 : 0;
}

int c0pq_migration_initialize(
    const c0pq_migration_value *initial_value,
    c0pq_journal_read_fn read_record,
    c0pq_journal_write_fn write_record,
    void *storage_context,
    const uint8_t journal_key[32]
)
{
    c0pq_migration_value encoded_value;
    c0pq_migration_value verified;
    uint8_t existing[C0PQ_MIGRATION_RECORD_BYTES];
    uint8_t raw[C0PQ_MIGRATION_RECORD_BYTES];
    uint8_t verify_raw[C0PQ_MIGRATION_RECORD_BYTES];
    int slot0_status;
    int slot1_status;
    int result = C0PQLINK_OK;
    if (initial_value == NULL || read_record == NULL
        || write_record == NULL || journal_key == NULL
        || initial_value->active_key_slot > 1u
        || initial_value->state < C0PQ_MIGRATION_LEGACY_PSK
        || initial_value->state
            > C0PQ_MIGRATION_PQ_REQUIRED_WITH_PSK_AUTH) {
        return C0PQLINK_ERR_ARGUMENT;
    }
    slot0_status = read_record(storage_context, 0u, existing);
    slot1_status = read_record(storage_context, 1u, existing);
    if (slot0_status == C0PQ_JOURNAL_READ_OK
        || slot1_status == C0PQ_JOURNAL_READ_OK) {
        c0pqlink_secure_zero(existing, sizeof(existing));
        return C0PQLINK_ERR_STATE;
    }
    if (slot0_status != C0PQ_JOURNAL_READ_EMPTY
        || slot1_status != C0PQ_JOURNAL_READ_EMPTY) {
        c0pqlink_secure_zero(existing, sizeof(existing));
        return C0PQLINK_ERR_IO;
    }
    memcpy(&encoded_value, initial_value, sizeof(encoded_value));
    encoded_value.generation = 1u;
    encode_record(raw, &encoded_value, journal_key);
    if (write_record(storage_context, 0u, raw) != 0
        || read_record(storage_context, 0u, verify_raw)
            != C0PQ_JOURNAL_READ_OK
        || !c0pqlink_ct_equal(raw, verify_raw, sizeof(raw))
        || decode_record(&verified, verify_raw, journal_key)
            != C0PQLINK_OK) {
        result = C0PQLINK_ERR_IO;
    }
    c0pqlink_secure_zero(&encoded_value, sizeof(encoded_value));
    c0pqlink_secure_zero(&verified, sizeof(verified));
    c0pqlink_secure_zero(existing, sizeof(existing));
    c0pqlink_secure_zero(raw, sizeof(raw));
    c0pqlink_secure_zero(verify_raw, sizeof(verify_raw));
    return result;
}

int c0pq_migration_commit(
    const c0pq_migration_value *next_value,
    c0pq_journal_read_fn read_record,
    c0pq_journal_write_fn write_record,
    void *storage_context,
    const uint8_t journal_key[32]
)
{
    c0pq_migration_value current;
    c0pq_migration_value encoded_value;
    c0pq_migration_value verified;
    uint8_t current_slot = 0u;
    uint8_t target_slot;
    uint8_t raw[C0PQ_MIGRATION_RECORD_BYTES];
    uint8_t verify_raw[C0PQ_MIGRATION_RECORD_BYTES];
    int result;
    if (next_value == NULL || read_record == NULL || write_record == NULL
        || journal_key == NULL || next_value->active_key_slot > 1u
        || next_value->state < C0PQ_MIGRATION_LEGACY_PSK
        || next_value->state
            > C0PQ_MIGRATION_PQ_REQUIRED_WITH_PSK_AUTH) {
        return C0PQLINK_ERR_ARGUMENT;
    }
    result = c0pq_migration_load(
        &current,
        &current_slot,
        read_record,
        storage_context,
        journal_key
    );
    if (result != C0PQLINK_OK) {
        c0pqlink_secure_zero(&current, sizeof(current));
        return result;
    }
    memcpy(&encoded_value, next_value, sizeof(encoded_value));
    if (!c0pq_migration_transition_allowed(
            current.state,
            next_value->state
        )
        || next_value->epoch < current.epoch
        || current.generation == UINT32_MAX) {
        c0pqlink_secure_zero(&current, sizeof(current));
        return C0PQLINK_ERR_STATE;
    }
    encoded_value.generation = current.generation + 1u;
    target_slot = (uint8_t)(current_slot ^ 1u);
    encode_record(raw, &encoded_value, journal_key);
    result = write_record(storage_context, target_slot, raw) == 0
        ? C0PQLINK_OK : C0PQLINK_ERR_IO;
    if (result == C0PQLINK_OK) {
        if (read_record(storage_context, target_slot, verify_raw)
                != C0PQ_JOURNAL_READ_OK
            || !c0pqlink_ct_equal(raw, verify_raw, sizeof(raw))
            || decode_record(
                &verified,
                verify_raw,
                journal_key
            ) != C0PQLINK_OK) {
            result = C0PQLINK_ERR_IO;
        }
    }
    c0pqlink_secure_zero(&current, sizeof(current));
    c0pqlink_secure_zero(&encoded_value, sizeof(encoded_value));
    c0pqlink_secure_zero(&verified, sizeof(verified));
    c0pqlink_secure_zero(raw, sizeof(raw));
    c0pqlink_secure_zero(verify_raw, sizeof(verify_raw));
    return result;
}
