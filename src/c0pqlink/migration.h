#ifndef C0PQLINK_MIGRATION_H
#define C0PQLINK_MIGRATION_H

#include "c0pqlink/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define C0PQ_MIGRATION_RECORD_BYTES 52u
#define C0PQ_JOURNAL_READ_OK 0
#define C0PQ_JOURNAL_READ_EMPTY 1

typedef enum {
    C0PQ_MIGRATION_LEGACY_PSK = 0,
    C0PQ_MIGRATION_PSK_PLUS_PQ = 1,
    C0PQ_MIGRATION_PQ_REQUIRED_WITH_PSK_AUTH = 2
} c0pq_migration_state;

typedef struct {
    uint32_t generation;
    c0pq_migration_state state;
    uint8_t active_key_slot;
    uint64_t epoch;
    uint8_t key_id[C0PQ_KEY_ID_BYTES];
} c0pq_migration_value;

typedef int (*c0pq_journal_read_fn)(
    void *context,
    uint8_t slot,
    uint8_t output[C0PQ_MIGRATION_RECORD_BYTES]
);

/*
 * read_record returns C0PQ_JOURNAL_READ_OK after filling all 52 bytes,
 * C0PQ_JOURNAL_READ_EMPTY only for an erased/uninitialized slot, and a
 * negative value for an I/O failure.
 */

typedef int (*c0pq_journal_write_fn)(
    void *context,
    uint8_t slot,
    const uint8_t record[C0PQ_MIGRATION_RECORD_BYTES]
);

int c0pq_migration_load(
    c0pq_migration_value *value,
    uint8_t *record_slot,
    c0pq_journal_read_fn read_record,
    void *storage_context,
    const uint8_t journal_key[32]
);

/*
 * First-provisioning operation. It succeeds only when neither storage slot
 * is readable, writes generation 1 to slot 0, and verifies the exact record.
 * Normal updates must use c0pq_migration_commit().
 */
int c0pq_migration_initialize(
    const c0pq_migration_value *initial_value,
    c0pq_journal_read_fn read_record,
    c0pq_journal_write_fn write_record,
    void *storage_context,
    const uint8_t journal_key[32]
);

int c0pq_migration_commit(
    const c0pq_migration_value *next_value,
    c0pq_journal_read_fn read_record,
    c0pq_journal_write_fn write_record,
    void *storage_context,
    const uint8_t journal_key[32]
);

int c0pq_migration_transition_allowed(
    c0pq_migration_state current,
    c0pq_migration_state next
);

#ifdef __cplusplus
}
#endif

#endif
