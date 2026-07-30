#include "sha256.h"

#include <string.h>

#ifdef __AVR__
#include <avr/pgmspace.h>
#define QP_READ_K(i) pgm_read_dword(&QP_K[(i)])
static const uint32_t QP_K[64] PROGMEM = {
#else
#define QP_READ_K(i) QP_K[(i)]
static const uint32_t QP_K[64] = {
#endif
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
    0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
    0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
    0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
    0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
    0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
    0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
    0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
    0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
    0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
    0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
    0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
    0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
    0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL};

static uint32_t rotr32(uint32_t x, uint8_t n) {
  return (x >> n) | (x << (32U - n));
}

static uint32_t read_be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static void transform_schedule(uint32_t state[8], uint32_t *w) {
  uint32_t a, b, c, d, e, f, g, h;
  uint8_t i;

#ifdef QP_ABLATION_SHA256_W64
  for (i = 16U; i < 64U; ++i) {
    const uint32_t w15 = w[i - 15U];
    const uint32_t w2 = w[i - 2U];
    const uint32_t s0 = rotr32(w15, 7) ^ rotr32(w15, 18) ^ (w15 >> 3);
    const uint32_t s1 = rotr32(w2, 17) ^ rotr32(w2, 19) ^ (w2 >> 10);
    w[i] = w[i - 16U] + s0 + w[i - 7U] + s1;
  }
#endif
  a = state[0];
  b = state[1];
  c = state[2];
  d = state[3];
  e = state[4];
  f = state[5];
  g = state[6];
  h = state[7];

  for (i = 0; i < 64; ++i) {
    uint32_t word;
#ifdef QP_ABLATION_SHA256_W64
    word = w[i];
#else
    if (i >= 16U) {
      const uint32_t w15 = w[(i + 1U) & 15U];
      const uint32_t w2 = w[(i + 14U) & 15U];
      const uint32_t s0 = rotr32(w15, 7) ^ rotr32(w15, 18) ^ (w15 >> 3);
      const uint32_t s1 = rotr32(w2, 17) ^ rotr32(w2, 19) ^ (w2 >> 10);
      w[i & 15U] += s0 + w[(i + 9U) & 15U] + s1;
    }
    word = w[i & 15U];
#endif
    const uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
    const uint32_t ch = (e & f) ^ ((~e) & g);
    const uint32_t t1 = h + s1 + ch + QP_READ_K(i) + word;
    const uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
    const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t t2 = s0 + maj;
    h = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + t2;
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}

static void transform(qp_sha256_ctx *ctx) {
#ifdef QP_ABLATION_SHA256_W64
  /*
   * Controlled ablation: the conventional 64-word schedule is kept only
   * for reproducible memory comparison with the NanoSLH rolling schedule.
  */
  uint32_t w[64];
#elif defined(QP_ABLATION_SEPARATE_SHA_SCHEDULE)
  /*
   * Controlled ablation for keeping a second 16-word schedule instead of
   * reusing the context block.
   */
  uint32_t w[16];
#endif
  uint8_t i;

  for (i = 0; i < 16; ++i) {
    const uint32_t word =
        read_be32(ctx->block.bytes + ((uint16_t)i * 4U));
#if defined(QP_ABLATION_SHA256_W64) || \
    defined(QP_ABLATION_SEPARATE_SHA_SCHEDULE)
    w[i] = word;
#else
    ctx->block.words[i] = word;
#endif
  }
#if defined(QP_ABLATION_SHA256_W64) || \
    defined(QP_ABLATION_SEPARATE_SHA_SCHEDULE)
  transform_schedule(ctx->state, w);
#else
  transform_schedule(ctx->state, ctx->block.words);
#endif
}

void qp_sha256_init(qp_sha256_ctx *ctx) {
  ctx->state[0] = 0x6a09e667UL;
  ctx->state[1] = 0xbb67ae85UL;
  ctx->state[2] = 0x3c6ef372UL;
  ctx->state[3] = 0xa54ff53aUL;
  ctx->state[4] = 0x510e527fUL;
  ctx->state[5] = 0x9b05688cUL;
  ctx->state[6] = 0x1f83d9abUL;
  ctx->state[7] = 0x5be0cd19UL;
  ctx->total_bytes = 0;
  ctx->block_len = 0;
}

void qp_sha256_fips205_seed(uint32_t state[8],
                            const uint8_t pk_seed[16]) {
#ifdef QP_ABLATION_SHA256_W64
  uint32_t w[64];
#else
  uint32_t w[16];
#endif
  uint8_t i;

  state[0] = 0x6a09e667UL;
  state[1] = 0xbb67ae85UL;
  state[2] = 0x3c6ef372UL;
  state[3] = 0xa54ff53aUL;
  state[4] = 0x510e527fUL;
  state[5] = 0x9b05688cUL;
  state[6] = 0x1f83d9abUL;
  state[7] = 0x5be0cd19UL;
  for (i = 0U; i < 4U; ++i) {
    w[i] = read_be32(pk_seed + ((uint16_t)i * 4U));
  }
  for (; i < 16U; ++i) {
    w[i] = 0U;
  }
  transform_schedule(state, w);
}

void qp_sha256_update(qp_sha256_ctx *ctx, const uint8_t *data, size_t len) {
  while (len != 0U) {
    const uint8_t room = (uint8_t)(64U - ctx->block_len);
    const uint8_t take = (len < room) ? (uint8_t)len : room;
    memcpy(ctx->block.bytes + ctx->block_len, data, take);
    ctx->block_len = (uint8_t)(ctx->block_len + take);
    ctx->total_bytes += take;
    data += take;
    len -= take;
    if (ctx->block_len == 64U) {
      transform(ctx);
      ctx->block_len = 0;
    }
  }
}

static void final_transform(qp_sha256_ctx *ctx) {
  const uint32_t low_bits = ctx->total_bytes << 3;
  const uint32_t high_bits = ctx->total_bytes >> 29;

  ctx->block.bytes[ctx->block_len++] = 0x80;
  if (ctx->block_len > 56U) {
    while (ctx->block_len < 64U) {
      ctx->block.bytes[ctx->block_len++] = 0;
    }
    transform(ctx);
    ctx->block_len = 0;
  }
  while (ctx->block_len < 56U) {
    ctx->block.bytes[ctx->block_len++] = 0;
  }
  ctx->block.bytes[56] = (uint8_t)(high_bits >> 24);
  ctx->block.bytes[57] = (uint8_t)(high_bits >> 16);
  ctx->block.bytes[58] = (uint8_t)(high_bits >> 8);
  ctx->block.bytes[59] = (uint8_t)high_bits;
  ctx->block.bytes[60] = (uint8_t)(low_bits >> 24);
  ctx->block.bytes[61] = (uint8_t)(low_bits >> 16);
  ctx->block.bytes[62] = (uint8_t)(low_bits >> 8);
  ctx->block.bytes[63] = (uint8_t)low_bits;
  transform(ctx);
}

static void write_digest_words(const qp_sha256_ctx *ctx, uint8_t *digest,
                               uint8_t words) {
  uint8_t i;
  for (i = 0; i < words; ++i) {
    digest[(uint16_t)i * 4U] = (uint8_t)(ctx->state[i] >> 24);
    digest[(uint16_t)i * 4U + 1U] = (uint8_t)(ctx->state[i] >> 16);
    digest[(uint16_t)i * 4U + 2U] = (uint8_t)(ctx->state[i] >> 8);
    digest[(uint16_t)i * 4U + 3U] = (uint8_t)ctx->state[i];
  }
}

void qp_sha256_final(qp_sha256_ctx *ctx, uint8_t digest[32]) {
  final_transform(ctx);
  write_digest_words(ctx, digest, 8U);
}

void qp_sha256_final_16(qp_sha256_ctx *ctx, uint8_t digest[16]) {
  final_transform(ctx);
  write_digest_words(ctx, digest, 4U);
}

void qp_sha256(const uint8_t *data, size_t len, uint8_t digest[32]) {
  qp_sha256_ctx ctx;
  qp_sha256_init(&ctx);
  qp_sha256_update(&ctx, data, len);
  qp_sha256_final(&ctx, digest);
}

#ifndef QP_ABLATION_HMAC_BUFFERS
static void hmac_prefix(qp_sha256_ctx *ctx, const uint8_t key[32],
                        uint8_t pad_byte) {
  uint8_t i;
  qp_sha256_init(ctx);
  for (i = 0U; i < 32U; ++i) {
    ctx->block.bytes[i] = (uint8_t)(key[i] ^ pad_byte);
  }
  memset(ctx->block.bytes + 32U, pad_byte, 32U);
  ctx->total_bytes = 64U;
  transform(ctx);
}
#endif

void qp_hmac_sha256_32(const uint8_t key[32], const uint8_t *message,
                       size_t message_len, uint8_t tag[32]) {
#ifdef QP_ABLATION_HMAC_BUFFERS
  uint8_t pad[64];
  uint8_t inner[32];
#endif
  qp_sha256_ctx ctx;
#ifdef QP_ABLATION_HMAC_BUFFERS
  uint8_t i;

  memset(pad, 0x36, sizeof(pad));
  for (i = 0; i < 32U; ++i) {
    pad[i] ^= key[i];
  }
  qp_sha256_init(&ctx);
  qp_sha256_update(&ctx, pad, sizeof(pad));
  qp_sha256_update(&ctx, message, message_len);
  qp_sha256_final(&ctx, inner);

  memset(pad, 0x5c, sizeof(pad));
  for (i = 0; i < 32U; ++i) {
    pad[i] ^= key[i];
  }
  qp_sha256_init(&ctx);
  qp_sha256_update(&ctx, pad, sizeof(pad));
  qp_sha256_update(&ctx, inner, sizeof(inner));
  qp_sha256_final(&ctx, tag);

  memset(inner, 0, sizeof(inner));
  memset(pad, 0, sizeof(pad));
#else
  hmac_prefix(&ctx, key, 0x36U);
  qp_sha256_update(&ctx, message, message_len);
  qp_sha256_final(&ctx, tag);

  hmac_prefix(&ctx, key, 0x5cU);
  qp_sha256_update(&ctx, tag, 32U);
  qp_sha256_final(&ctx, tag);
#endif
}

int qp_constant_time_equal(const uint8_t *a, const uint8_t *b, size_t len) {
  uint8_t diff = 0;
  size_t i;
  for (i = 0; i < len; ++i) {
    diff |= (uint8_t)(a[i] ^ b[i]);
  }
  return diff == 0;
}
