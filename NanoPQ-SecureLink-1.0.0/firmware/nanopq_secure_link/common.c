#include "common.h"

void npq_secure_zero(void *pointer, size_t length)
{
    volatile uint8_t *bytes = (volatile uint8_t *)pointer;
    while (length-- != 0u) {
        *bytes++ = 0u;
    }
}

int npq_constant_time_equal(
    const uint8_t *left,
    const uint8_t *right,
    size_t length
)
{
    uint8_t difference = 0u;
    size_t index;
    for (index = 0u; index < length; ++index) {
        difference |= (uint8_t)(left[index] ^ right[index]);
    }
    return difference == 0u;
}

void npq_store_u32_be(uint8_t output[4], uint32_t value)
{
    output[0] = (uint8_t)(value >> 24u);
    output[1] = (uint8_t)(value >> 16u);
    output[2] = (uint8_t)(value >> 8u);
    output[3] = (uint8_t)value;
}

uint32_t npq_load_u32_be(const uint8_t input[4])
{
    return ((uint32_t)input[0] << 24u)
        | ((uint32_t)input[1] << 16u)
        | ((uint32_t)input[2] << 8u)
        | (uint32_t)input[3];
}
