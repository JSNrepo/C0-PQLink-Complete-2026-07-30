#ifndef C0PQLINK_COMMON_H
#define C0PQLINK_COMMON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define C0PQLINK_VERSION_MAJOR 0
#define C0PQLINK_VERSION_MINOR 1
#define C0PQLINK_VERSION_PATCH 0

#define C0PQLINK_OK 0
#define C0PQLINK_ERR_ARGUMENT -1
#define C0PQLINK_ERR_KEY_ENCODING -2
#define C0PQLINK_ERR_IO -3
#define C0PQLINK_ERR_AUTH -4
#define C0PQLINK_ERR_STATE -5
#define C0PQLINK_ERR_REPLAY -6
#define C0PQLINK_ERR_EXPIRED -7
#define C0PQLINK_ERR_CAPACITY -8
#define C0PQLINK_ERR_RNG -9

void c0pqlink_secure_zero(void *ptr, size_t len);
int c0pqlink_ct_equal(const uint8_t *a, const uint8_t *b, size_t len);

#ifdef __cplusplus
}
#endif

#endif

