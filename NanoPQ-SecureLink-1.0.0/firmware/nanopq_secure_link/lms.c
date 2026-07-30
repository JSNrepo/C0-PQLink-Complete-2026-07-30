#include "lms.h"

#include "sha256.h"

#include <string.h>

#define NPQ_LMS_TYPE_SHA256_M32_H5 5u
#define NPQ_LMOTS_TYPE_SHA256_N32_W4 3u
#define NPQ_LMOTS_TYPE_SHA256_N32_W8 4u
#define NPQ_LMS_HEIGHT 5u

static uint32_t load_u32_be(const uint8_t input[4])
{
    return ((uint32_t)input[0] << 24u)
        | ((uint32_t)input[1] << 16u)
        | ((uint32_t)input[2] << 8u)
        | input[3];
}

static void store_u16_be(uint8_t output[2], uint16_t value)
{
    output[0] = (uint8_t)(value >> 8u);
    output[1] = (uint8_t)value;
}

static void store_u32_be(uint8_t output[4], uint32_t value)
{
    output[0] = (uint8_t)(value >> 24u);
    output[1] = (uint8_t)(value >> 16u);
    output[2] = (uint8_t)(value >> 8u);
    output[3] = (uint8_t)value;
}

static int read_part(
    npq_lms_read_fn reader,
    void *reader_context,
    uint8_t *output,
    size_t length,
    size_t *bytes_consumed
)
{
    if (!reader(reader_context, output, length)) {
        return 0;
    }
    if (bytes_consumed != NULL) {
        *bytes_consumed += length;
    }
    return 1;
}

static void hash_chain_step(
    uint8_t output[32],
    const uint8_t identifier[16],
    const uint8_t encoded_q[4],
    uint16_t chain_index,
    uint8_t iteration,
    const uint8_t input[32]
)
{
    uint8_t block[55];
    memcpy(block, identifier, 16u);
    memcpy(block + 16u, encoded_q, 4u);
    store_u16_be(block + 20u, chain_index);
    block[22] = iteration;
    memcpy(block + 23u, input, 32u);
    qp_sha256(block, sizeof(block), output);
    memset(block, 0, sizeof(block));
}

static void hash_lms_node(
    uint8_t output[32],
    const uint8_t identifier[16],
    uint32_t node_number,
    uint16_t domain,
    const uint8_t left[32],
    const uint8_t *right
)
{
    uint8_t block[86];
    const size_t length = right == NULL ? 54u : 86u;
    memcpy(block, identifier, 16u);
    store_u32_be(block + 16u, node_number);
    store_u16_be(block + 20u, domain);
    memcpy(block + 22u, left, 32u);
    if (right != NULL) {
        memcpy(block + 54u, right, 32u);
    }
    qp_sha256(block, length, output);
    memset(block, 0, sizeof(block));
}

static uint8_t coefficient_at(
    const uint8_t message_hash[32],
    const uint8_t checksum[2],
    uint16_t index,
    uint8_t width
)
{
    const uint16_t message_coefficients = (uint16_t)(256u / width);
    const uint16_t local_index = index < message_coefficients
        ? index
        : (uint16_t)(index - message_coefficients);
    const uint8_t *bytes = index < message_coefficients
        ? message_hash
        : checksum;
    const uint8_t coefficients_per_byte = (uint8_t)(8u / width);
    const uint8_t shift = (uint8_t)(
        8u - width
        - width * (uint8_t)(local_index % coefficients_per_byte)
    );
    return (uint8_t)(
        (bytes[local_index / coefficients_per_byte] >> shift)
        & ((1u << width) - 1u)
    );
}

npq_lms_result npq_lms_sha256_m32_h5_verify_stream(
    const uint8_t public_key[NPQ_LMS_PUBLIC_KEY_BYTES],
    const uint8_t *message,
    size_t message_length,
    size_t signature_length,
    npq_lms_read_fn reader,
    void *reader_context,
    size_t *bytes_consumed
)
{
    static const uint8_t d_message[2] = { 0x81u, 0x81u };
    static const uint8_t d_public[2] = { 0x80u, 0x80u };
    uint8_t encoded_q[4];
    uint8_t encoded_type[4];
    uint8_t randomizer[32];
    uint8_t message_hash[32];
    uint8_t chain_value[32];
    uint8_t chain_output[32];
    uint8_t candidate_ots_key[32];
    uint8_t sibling[32];
    uint8_t node_output[32];
    uint8_t checksum_bytes[2];
    qp_sha256_ctx hash;
    uint32_t q;
    uint32_t node_number;
    uint16_t checksum = 0u;
    uint16_t chain_index;
    uint8_t iteration;
    uint8_t coefficient;
    uint8_t level;
    uint8_t width;
    uint8_t chain_limit;
    uint16_t p;
    uint16_t checksum_shift;
    size_t expected_signature_length;
    uint32_t ots_type;
    const uint8_t *identifier;

    if (bytes_consumed != NULL) {
        *bytes_consumed = 0u;
    }
    if (public_key == NULL
        || (message == NULL && message_length != 0u)
        || reader == NULL) {
        return NPQ_LMS_BAD_PARAMETER;
    }
    ots_type = load_u32_be(public_key + 4u);
    if (ots_type == NPQ_LMOTS_TYPE_SHA256_N32_W4) {
        width = 4u;
        p = 67u;
        checksum_shift = 4u;
        expected_signature_length = NPQ_LMS_W4_SIGNATURE_BYTES;
    } else if (ots_type == NPQ_LMOTS_TYPE_SHA256_N32_W8) {
        width = 8u;
        p = 34u;
        checksum_shift = 0u;
        expected_signature_length = NPQ_LMS_W8_SIGNATURE_BYTES;
    } else {
        return NPQ_LMS_BAD_PARAMETER;
    }
    if (signature_length != expected_signature_length) {
        return NPQ_LMS_BAD_LENGTH;
    }
    if (load_u32_be(public_key) != NPQ_LMS_TYPE_SHA256_M32_H5) {
        return NPQ_LMS_BAD_PARAMETER;
    }
    chain_limit = (uint8_t)((1u << width) - 1u);
    identifier = public_key + 8u;
    if (!read_part(
            reader,
            reader_context,
            encoded_q,
            sizeof(encoded_q),
            bytes_consumed
        )
        || !read_part(
            reader,
            reader_context,
            encoded_type,
            sizeof(encoded_type),
            bytes_consumed
        )
        || !read_part(
            reader,
            reader_context,
            randomizer,
            sizeof(randomizer),
            bytes_consumed
        )) {
        return NPQ_LMS_IO_ERROR;
    }
    q = load_u32_be(encoded_q);
    if (q >= (UINT32_C(1) << NPQ_LMS_HEIGHT)
        || load_u32_be(encoded_type) != ots_type) {
        return NPQ_LMS_BAD_SIGNATURE;
    }

    qp_sha256_init(&hash);
    qp_sha256_update(&hash, identifier, 16u);
    qp_sha256_update(&hash, encoded_q, sizeof(encoded_q));
    qp_sha256_update(&hash, d_message, sizeof(d_message));
    qp_sha256_update(&hash, randomizer, sizeof(randomizer));
    qp_sha256_update(&hash, message, message_length);
    qp_sha256_final(&hash, message_hash);
    for (
        chain_index = 0u;
        chain_index < (uint16_t)(256u / width);
        ++chain_index
    ) {
        checksum = (uint16_t)(
            checksum + chain_limit
            - coefficient_at(
                message_hash,
                checksum_bytes,
                chain_index,
                width
            )
        );
    }
    checksum = (uint16_t)(checksum << checksum_shift);
    store_u16_be(checksum_bytes, checksum);

    qp_sha256_init(&hash);
    qp_sha256_update(&hash, identifier, 16u);
    qp_sha256_update(&hash, encoded_q, sizeof(encoded_q));
    qp_sha256_update(&hash, d_public, sizeof(d_public));
    for (chain_index = 0u; chain_index < p; ++chain_index) {
        if (!read_part(
                reader,
                reader_context,
                chain_value,
                sizeof(chain_value),
                bytes_consumed
            )) {
            return NPQ_LMS_IO_ERROR;
        }
        coefficient = coefficient_at(
            message_hash,
            checksum_bytes,
            chain_index,
            width
        );
        for (
            iteration = coefficient;
            iteration < chain_limit;
            ++iteration
        ) {
            hash_chain_step(
                chain_output,
                identifier,
                encoded_q,
                chain_index,
                iteration,
                chain_value
            );
            memcpy(chain_value, chain_output, sizeof(chain_value));
        }
        qp_sha256_update(&hash, chain_value, sizeof(chain_value));
    }
    qp_sha256_final(&hash, candidate_ots_key);

    if (!read_part(
            reader,
            reader_context,
            encoded_type,
            sizeof(encoded_type),
            bytes_consumed
        )
        || load_u32_be(encoded_type)
            != NPQ_LMS_TYPE_SHA256_M32_H5) {
        return NPQ_LMS_BAD_SIGNATURE;
    }
    node_number = (UINT32_C(1) << NPQ_LMS_HEIGHT) + q;
    hash_lms_node(
        chain_value,
        identifier,
        node_number,
        UINT16_C(0x8282),
        candidate_ots_key,
        NULL
    );
    for (level = 0u; level < NPQ_LMS_HEIGHT; ++level) {
        if (!read_part(
                reader,
                reader_context,
                sibling,
                sizeof(sibling),
                bytes_consumed
            )) {
            return NPQ_LMS_IO_ERROR;
        }
        if ((node_number & 1u) != 0u) {
            hash_lms_node(
                node_output,
                identifier,
                node_number / 2u,
                UINT16_C(0x8383),
                sibling,
                chain_value
            );
        } else {
            hash_lms_node(
                node_output,
                identifier,
                node_number / 2u,
                UINT16_C(0x8383),
                chain_value,
                sibling
            );
        }
        memcpy(chain_value, node_output, sizeof(chain_value));
        node_number /= 2u;
    }
    memset(&hash, 0, sizeof(hash));
    memset(randomizer, 0, sizeof(randomizer));
    memset(message_hash, 0, sizeof(message_hash));
    memset(chain_output, 0, sizeof(chain_output));
    memset(candidate_ots_key, 0, sizeof(candidate_ots_key));
    memset(sibling, 0, sizeof(sibling));
    memset(node_output, 0, sizeof(node_output));
    return qp_constant_time_equal(
        chain_value,
        public_key + 24u,
        32u
    ) ? NPQ_LMS_OK : NPQ_LMS_BAD_SIGNATURE;
}
