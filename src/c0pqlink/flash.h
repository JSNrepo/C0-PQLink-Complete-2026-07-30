#ifndef C0PQLINK_FLASH_H
#define C0PQLINK_FLASH_H

#include <stdint.h>

/*
 * AVR is a Harvard architecture: an ordinary C `const` object is copied into
 * SRAM at reset.  C0PQLINK_FLASH keeps immutable tables in program flash and
 * these readers make the same sources work on unified-address-space targets.
 */
#if defined(__AVR__)
#include <avr/pgmspace.h>
#define C0PQLINK_FLASH PROGMEM

static inline uint8_t c0pqlink_flash_read_u8(const uint8_t *address)
{
    return pgm_read_byte(address);
}

static inline uint16_t c0pqlink_flash_read_u16(const uint16_t *address)
{
    return pgm_read_word(address);
}

static inline uint32_t c0pqlink_flash_read_u32(const uint32_t *address)
{
    return pgm_read_dword(address);
}

static inline uint64_t c0pqlink_flash_read_u64(const uint64_t *address)
{
    const uint8_t *bytes = (const uint8_t *)(const void *)address;
    uint64_t value = 0u;
    unsigned int i;
    for (i = 0u; i < 8u; ++i) {
        value |= ((uint64_t)pgm_read_byte(bytes + i)) << (8u * i);
    }
    return value;
}
#else
#define C0PQLINK_FLASH

static inline uint8_t c0pqlink_flash_read_u8(const uint8_t *address)
{
    return *address;
}

static inline uint16_t c0pqlink_flash_read_u16(const uint16_t *address)
{
    return *address;
}

static inline uint32_t c0pqlink_flash_read_u32(const uint32_t *address)
{
    return *address;
}

static inline uint64_t c0pqlink_flash_read_u64(const uint64_t *address)
{
    return *address;
}
#endif

#endif
