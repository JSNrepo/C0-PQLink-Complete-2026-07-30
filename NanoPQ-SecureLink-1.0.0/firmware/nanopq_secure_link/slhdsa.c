/*
 * Specialized streaming verification for FIPS 205
 * SLH-DSA-SHA2-128s.
 *
 * The control flow follows Algorithms 4, 5, 8, 11, 13, 17, 20, and 24 of
 * FIPS 205.  The SHA-2 instantiation and compressed-address rules follow
 * Sections 11.1 and 11.2.  This verifier was independently checked against
 * NIST ACVP vectors and signatures produced by the pinned slhdsa-c project.
 *
 * NanoSLH specializes the live-value schedule, not the cryptographic
 * primitive: signature values are consumed and folded in wire order, WOTS
 * digits and FORS indices are derived at point of use, the SHA2 compressed
 * address is kept natively, and only a compact fixed-prefix SHA state is
 * retained. Compile-time QP_ABLATION_* switches rebuild the less compact
 * schedules used by the reproducible optimization benchmark.
 *
 * The implementation approach was informed by slhdsa-c, whose authors make
 * that code available under Apache-2.0 OR ISC OR MIT.  See
 * third_party/slhdsa-c/LICENSE and THIRD_PARTY_NOTICES.md.
 */

#include "slhdsa.h"

#include "sha256.h"

#include <string.h>

#define SLH_N 16U
#define SLH_H 63U
#define SLH_D 7U
#define SLH_HP 9U
#define SLH_A 12U
#define SLH_K 14U
#define SLH_WOTS_LEN 35U
#define SLH_MD_BYTES 21U
#define SLH_DIGEST_BYTES 30U

#define ADRS_WOTS_HASH 0U
#define ADRS_WOTS_PK 1U
#define ADRS_TREE 2U
#define ADRS_FORS_TREE 3U
#define ADRS_FORS_ROOTS 4U

typedef struct {
#ifdef QP_ABLATION_FULL_ADDRESS
  uint8_t bytes[32];
#else
  uint8_t bytes[22];
#endif
} slh_address;

typedef struct {
  const uint8_t *pk_seed;
  const uint8_t *pk_root;
  slh_address address;
#ifdef QP_ABLATION_FULL_SEEDED_CONTEXT
  qp_sha256_ctx seeded_sha256;
#else
  uint32_t seeded_sha256_state[8];
#endif
  qp_slhdsa_read_fn reader;
  void *reader_context;
  size_t consumed;
} slh_verify_ctx;

static void write_be32(uint8_t out[4], uint32_t value) {
  out[0] = (uint8_t)(value >> 24);
  out[1] = (uint8_t)(value >> 16);
  out[2] = (uint8_t)(value >> 8);
  out[3] = (uint8_t)value;
}

static void write_be64(uint8_t out[8], uint64_t value) {
  uint8_t i;
  for (i = 0U; i < 8U; ++i) {
    out[7U - i] = (uint8_t)value;
    value >>= 8;
  }
}

static uint64_t read_be_uint(const uint8_t *in, uint8_t length) {
  uint64_t value = 0U;
  while (length-- != 0U) {
    value = (value << 8) | *in++;
  }
  return value;
}

static void address_zero(slh_verify_ctx *ctx) {
  memset(ctx->address.bytes, 0, sizeof(ctx->address.bytes));
}

static void address_set_layer(slh_verify_ctx *ctx, uint32_t layer) {
#ifdef QP_ABLATION_FULL_ADDRESS
  write_be32(ctx->address.bytes, layer);
#else
  ctx->address.bytes[0] = (uint8_t)layer;
#endif
}

static void address_set_tree(slh_verify_ctx *ctx, uint64_t tree) {
#ifdef QP_ABLATION_FULL_ADDRESS
  memset(ctx->address.bytes + 4U, 0, 4U);
  write_be64(ctx->address.bytes + 8U, tree);
#else
  write_be64(ctx->address.bytes + 1U, tree);
#endif
}

static void address_set_type_and_clear(slh_verify_ctx *ctx, uint32_t type) {
#ifdef QP_ABLATION_FULL_ADDRESS
  write_be32(ctx->address.bytes + 16U, type);
  memset(ctx->address.bytes + 20U, 0, 12U);
#else
  ctx->address.bytes[9] = (uint8_t)type;
  memset(ctx->address.bytes + 10U, 0, 12U);
#endif
}

static void address_set_type_and_clear_not_keypair(slh_verify_ctx *ctx,
                                                    uint32_t type) {
#ifdef QP_ABLATION_FULL_ADDRESS
  write_be32(ctx->address.bytes + 16U, type);
  memset(ctx->address.bytes + 24U, 0, 8U);
#else
  ctx->address.bytes[9] = (uint8_t)type;
  memset(ctx->address.bytes + 14U, 0, 8U);
#endif
}

static void address_set_keypair(slh_verify_ctx *ctx, uint32_t keypair) {
#ifdef QP_ABLATION_FULL_ADDRESS
  write_be32(ctx->address.bytes + 20U, keypair);
#else
  write_be32(ctx->address.bytes + 10U, keypair);
#endif
}

static void address_set_chain_or_height(slh_verify_ctx *ctx, uint32_t value) {
#ifdef QP_ABLATION_FULL_ADDRESS
  write_be32(ctx->address.bytes + 24U, value);
#else
  write_be32(ctx->address.bytes + 14U, value);
#endif
}

static void address_set_hash_or_index(slh_verify_ctx *ctx, uint32_t value) {
#ifdef QP_ABLATION_FULL_ADDRESS
  write_be32(ctx->address.bytes + 28U, value);
#else
  write_be32(ctx->address.bytes + 18U, value);
#endif
}

#ifdef QP_ABLATION_FULL_ADDRESS
static void address_compress(const slh_verify_ctx *ctx, uint8_t out[22]) {
  out[0] = ctx->address.bytes[3];
  memcpy(out + 1U, ctx->address.bytes + 8U, 8U);
  memcpy(out + 9U, ctx->address.bytes + 19U, 13U);
}
#endif

static int read_signature(slh_verify_ctx *ctx, uint8_t *out, size_t len) {
  if (!ctx->reader(ctx->reader_context, out, len)) {
    return 0;
  }
  ctx->consumed += len;
  return 1;
}

static void hash_start(const slh_verify_ctx *ctx, qp_sha256_ctx *hash) {
#ifdef QP_ABLATION_FULL_ADDRESS
  uint8_t compressed[22];
#endif
#ifdef QP_ABLATION_FULL_SEEDED_CONTEXT
  *hash = ctx->seeded_sha256;
#else
  memcpy(hash->state, ctx->seeded_sha256_state,
         sizeof(ctx->seeded_sha256_state));
  hash->total_bytes = 64U;
  hash->block_len = 0U;
#endif
#ifdef QP_ABLATION_FULL_ADDRESS
  address_compress(ctx, compressed);
  qp_sha256_update(hash, compressed, sizeof(compressed));
#else
  qp_sha256_update(hash, ctx->address.bytes, sizeof(ctx->address.bytes));
#endif
}

static void hash_finish_n(qp_sha256_ctx *hash, uint8_t out[SLH_N]) {
#ifdef QP_ABLATION_FULL_HASH_DIGEST
  uint8_t digest[32];
  qp_sha256_final(hash, digest);
  memcpy(out, digest, SLH_N);
#else
  qp_sha256_final_16(hash, out);
#endif
}

static void hash_f(const slh_verify_ctx *ctx, uint8_t out[SLH_N],
                   const uint8_t in[SLH_N]) {
  qp_sha256_ctx hash;
  hash_start(ctx, &hash);
  qp_sha256_update(&hash, in, SLH_N);
  hash_finish_n(&hash, out);
}

static void hash_h(const slh_verify_ctx *ctx, uint8_t out[SLH_N],
                   const uint8_t left[SLH_N],
                   const uint8_t right[SLH_N]) {
  qp_sha256_ctx hash;
  hash_start(ctx, &hash);
  qp_sha256_update(&hash, left, SLH_N);
  qp_sha256_update(&hash, right, SLH_N);
  hash_finish_n(&hash, out);
}

static void chain(slh_verify_ctx *ctx, uint8_t value[SLH_N], uint8_t start,
                  uint8_t steps) {
  uint8_t j;
  for (j = start; j < (uint8_t)(start + steps); ++j) {
    address_set_hash_or_index(ctx, j);
    hash_f(ctx, value, value);
  }
}

static uint16_t wots_checksum(const uint8_t message[SLH_N]) {
  uint16_t checksum = 0U;
  uint8_t i;
  for (i = 0U; i < 32U; ++i) {
    const uint8_t value = message[i >> 1U];
    const uint8_t digit =
        (i & 1U) ? (value & 0x0fU) : (value >> 4U);
    checksum = (uint16_t)(checksum + 15U - digit);
  }
  return (uint16_t)(checksum << 4U);
}

static uint8_t wots_digit_at(const uint8_t message[SLH_N],
                             uint16_t checksum, uint8_t index) {
  if (index < 32U) {
    const uint8_t value = message[index >> 1U];
    return (index & 1U) ? (value & 0x0fU) : (value >> 4U);
  }
  return (uint8_t)((checksum >> (12U - 4U * (index - 32U))) & 0x0fU);
}

static int xmss_public_key_from_signature(slh_verify_ctx *ctx,
                                          uint8_t root[SLH_N],
                                          uint32_t leaf_index,
                                          const uint8_t message[SLH_N]) {
  const uint16_t checksum = wots_checksum(message);
#ifdef QP_ABLATION_MATERIALIZED_STATE
  uint8_t digits[SLH_WOTS_LEN];
#endif
  uint8_t value[SLH_N];
  uint8_t sibling[SLH_N];
  qp_sha256_ctx aggregate;
  uint8_t i;

#ifdef QP_ABLATION_MATERIALIZED_STATE
  for (i = 0U; i < SLH_WOTS_LEN; ++i) {
    digits[i] = wots_digit_at(message, checksum, i);
  }
#endif

  address_set_type_and_clear_not_keypair(ctx, ADRS_WOTS_PK);
  hash_start(ctx, &aggregate);

  address_set_type_and_clear_not_keypair(ctx, ADRS_WOTS_HASH);
  for (i = 0U; i < SLH_WOTS_LEN; ++i) {
#ifdef QP_ABLATION_MATERIALIZED_STATE
    const uint8_t digit = digits[i];
#else
    const uint8_t digit = wots_digit_at(message, checksum, i);
#endif
    address_set_chain_or_height(ctx, i);
    if (!read_signature(ctx, value, sizeof(value))) {
      return 0;
    }
    chain(ctx, value, digit, (uint8_t)(15U - digit));
    qp_sha256_update(&aggregate, value, sizeof(value));
  }
  hash_finish_n(&aggregate, root);

  address_set_type_and_clear(ctx, ADRS_TREE);
  for (i = 0U; i < SLH_HP; ++i) {
    if (!read_signature(ctx, sibling, sizeof(sibling))) {
      return 0;
    }
    address_set_chain_or_height(ctx, (uint32_t)i + 1U);
    address_set_hash_or_index(ctx, leaf_index >> ((uint32_t)i + 1U));
    if (((leaf_index >> i) & 1U) == 0U) {
      hash_h(ctx, root, root, sibling);
    } else {
      hash_h(ctx, root, sibling, root);
    }
  }
  return 1;
}

static uint16_t fors_index_at(const uint8_t md[SLH_MD_BYTES],
                              uint8_t index) {
  const uint8_t byte_index = (uint8_t)(((uint16_t)index * 12U) >> 3U);
  if ((index & 1U) == 0U) {
    return (uint16_t)(((uint16_t)md[byte_index] << 4U) |
                      (md[byte_index + 1U] >> 4U));
  }
  return (uint16_t)((((uint16_t)md[byte_index] & 0x0fU) << 8U) |
                    md[byte_index + 1U]);
}

static int fors_public_key_from_signature(slh_verify_ctx *ctx,
                                          uint8_t public_key[SLH_N],
                                          const uint8_t md[SLH_MD_BYTES]) {
#ifdef QP_ABLATION_MATERIALIZED_STATE
  uint16_t indices[SLH_K];
#endif
  uint8_t node[SLH_N];
  uint8_t sibling[SLH_N];
  qp_sha256_ctx aggregate;
  uint8_t i;
  uint8_t j;

#ifdef QP_ABLATION_MATERIALIZED_STATE
  for (i = 0U; i < SLH_K; ++i) {
    indices[i] = fors_index_at(md, i);
  }
#endif

  address_set_type_and_clear_not_keypair(ctx, ADRS_FORS_ROOTS);
  hash_start(ctx, &aggregate);

  address_set_type_and_clear_not_keypair(ctx, ADRS_FORS_TREE);
  for (i = 0U; i < SLH_K; ++i) {
#ifdef QP_ABLATION_MATERIALIZED_STATE
    const uint16_t fors_index = indices[i];
#else
    const uint16_t fors_index = fors_index_at(md, i);
#endif
    uint32_t tree_index = ((uint32_t)i << SLH_A) + fors_index;
    address_set_chain_or_height(ctx, 0U);
    address_set_hash_or_index(ctx, tree_index);
    if (!read_signature(ctx, node, sizeof(node))) {
      return 0;
    }
    hash_f(ctx, node, node);

    for (j = 0U; j < SLH_A; ++j) {
      if (!read_signature(ctx, sibling, sizeof(sibling))) {
        return 0;
      }
      address_set_chain_or_height(ctx, (uint32_t)j + 1U);
      address_set_hash_or_index(ctx, tree_index >> ((uint32_t)j + 1U));
      if (((fors_index >> j) & 1U) == 0U) {
        hash_h(ctx, node, node, sibling);
      } else {
        hash_h(ctx, node, sibling, node);
      }
    }
    qp_sha256_update(&aggregate, node, sizeof(node));
  }
  hash_finish_n(&aggregate, public_key);
  return 1;
}

static void h_message(const slh_verify_ctx *ctx, uint8_t digest[32],
                      const uint8_t randomizer[SLH_N], const uint8_t *message,
                      size_t message_len, const uint8_t *context,
                      uint8_t context_len) {
  qp_sha256_ctx hash;
#ifdef QP_ABLATION_MATERIALIZED_STATE
  uint8_t inner[32];
#endif
  const uint8_t domain[2] = {0x00U, context_len};
  const uint8_t counter[4] = {0U, 0U, 0U, 0U};

  qp_sha256_init(&hash);
  qp_sha256_update(&hash, randomizer, SLH_N);
  qp_sha256_update(&hash, ctx->pk_seed, SLH_N);
  qp_sha256_update(&hash, ctx->pk_root, SLH_N);
  qp_sha256_update(&hash, domain, sizeof(domain));
  if (context_len != 0U) {
    qp_sha256_update(&hash, context, context_len);
  }
  qp_sha256_update(&hash, message, message_len);
#ifdef QP_ABLATION_MATERIALIZED_STATE
  qp_sha256_final(&hash, inner);
#else
  qp_sha256_final(&hash, digest);
#endif

  qp_sha256_init(&hash);
  qp_sha256_update(&hash, randomizer, SLH_N);
  qp_sha256_update(&hash, ctx->pk_seed, SLH_N);
#ifdef QP_ABLATION_MATERIALIZED_STATE
  qp_sha256_update(&hash, inner, sizeof(inner));
#else
  qp_sha256_update(&hash, digest, 32U);
#endif
  qp_sha256_update(&hash, counter, sizeof(counter));
  qp_sha256_final(&hash, digest);
}

qp_slhdsa_result qp_slhdsa_sha2_128s_verify_stream(
    const uint8_t public_key[QP_SLHDSA_PUBLIC_KEY_BYTES],
    const uint8_t *message, size_t message_len, const uint8_t *context,
    size_t context_len, size_t signature_len, qp_slhdsa_read_fn reader,
    void *reader_context, size_t *bytes_consumed) {
  slh_verify_ctx verify;
#ifdef QP_ABLATION_FULL_SEEDED_CONTEXT
  uint8_t zeros[16] = {0U};
#endif
  uint8_t randomizer[SLH_N];
  uint8_t digest[32];
  uint8_t node[SLH_N];
  uint64_t tree_index;
  uint32_t leaf_index;
  uint8_t layer;

  if (bytes_consumed != NULL) {
    *bytes_consumed = 0U;
  }
  if (signature_len != QP_SLHDSA_SIGNATURE_BYTES) {
    return QP_SLHDSA_BAD_LENGTH;
  }
  if (context_len > 255U || (context_len != 0U && context == NULL)) {
    return QP_SLHDSA_BAD_CONTEXT;
  }
  if (reader == NULL || public_key == NULL ||
      (message_len != 0U && message == NULL)) {
    return QP_SLHDSA_IO_ERROR;
  }

  verify.pk_seed = public_key;
  verify.pk_root = public_key + SLH_N;
  verify.reader = reader;
  verify.reader_context = reader_context;
  verify.consumed = 0U;
  address_zero(&verify);

#ifdef QP_ABLATION_FULL_SEEDED_CONTEXT
  qp_sha256_init(&verify.seeded_sha256);
  qp_sha256_update(&verify.seeded_sha256, verify.pk_seed, SLH_N);
  qp_sha256_update(&verify.seeded_sha256, zeros, sizeof(zeros));
  qp_sha256_update(&verify.seeded_sha256, zeros, sizeof(zeros));
  qp_sha256_update(&verify.seeded_sha256, zeros, sizeof(zeros));
#else
  qp_sha256_fips205_seed(verify.seeded_sha256_state, verify.pk_seed);
#endif

  if (!read_signature(&verify, randomizer, sizeof(randomizer))) {
    if (bytes_consumed != NULL) {
      *bytes_consumed = verify.consumed;
    }
    return QP_SLHDSA_IO_ERROR;
  }
  h_message(&verify, digest, randomizer, message, message_len, context,
            (uint8_t)context_len);

  tree_index = read_be_uint(digest + SLH_MD_BYTES, 7U);
  tree_index &= (UINT64_C(1) << (SLH_H - SLH_HP)) - UINT64_C(1);
  leaf_index =
      (uint32_t)read_be_uint(digest + SLH_MD_BYTES + 7U, 2U) & 0x1ffU;

  address_zero(&verify);
  address_set_tree(&verify, tree_index);
  address_set_type_and_clear_not_keypair(&verify, ADRS_FORS_TREE);
  address_set_keypair(&verify, leaf_index);
  if (!fors_public_key_from_signature(&verify, node, digest)) {
    if (bytes_consumed != NULL) {
      *bytes_consumed = verify.consumed;
    }
    return QP_SLHDSA_IO_ERROR;
  }

  for (layer = 0U; layer < SLH_D; ++layer) {
    address_zero(&verify);
    address_set_layer(&verify, layer);
    address_set_tree(&verify, tree_index);
    address_set_keypair(&verify, leaf_index);
    if (!xmss_public_key_from_signature(&verify, node, leaf_index, node)) {
      if (bytes_consumed != NULL) {
        *bytes_consumed = verify.consumed;
      }
      return QP_SLHDSA_IO_ERROR;
    }
    if (layer + 1U < SLH_D) {
      leaf_index = (uint32_t)(tree_index & 0x1ffU);
      tree_index >>= SLH_HP;
    }
  }

  if (bytes_consumed != NULL) {
    *bytes_consumed = verify.consumed;
  }
  if (verify.consumed != QP_SLHDSA_SIGNATURE_BYTES) {
    return QP_SLHDSA_BAD_LENGTH;
  }
  return qp_constant_time_equal(node, verify.pk_root, SLH_N)
             ? QP_SLHDSA_OK
             : QP_SLHDSA_BAD_SIGNATURE;
}
