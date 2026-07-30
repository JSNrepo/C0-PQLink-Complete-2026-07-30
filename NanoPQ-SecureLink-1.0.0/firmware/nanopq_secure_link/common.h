#ifndef NPQ_COMMON_H
#define NPQ_COMMON_H

#include <stddef.h>
#include <stdint.h>

#define NPQ_OK 0
#define NPQ_ERR_ARGUMENT -1
#define NPQ_ERR_FORMAT -2
#define NPQ_ERR_AUTH -3
#define NPQ_ERR_REPLAY -4
#define NPQ_ERR_STATE -5
#define NPQ_ERR_CAPACITY -6
#define NPQ_ERR_IO -7

void npq_secure_zero(void *pointer, size_t length);
int npq_constant_time_equal(
    const uint8_t *left,
    const uint8_t *right,
    size_t length
);
void npq_store_u32_be(uint8_t output[4], uint32_t value);
uint32_t npq_load_u32_be(const uint8_t input[4]);

#endif
