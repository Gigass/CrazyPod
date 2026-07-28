#ifndef TEST_MINIAPP_CRC32_H
#define TEST_MINIAPP_CRC32_H

#include <stddef.h>
#include <stdint.h>

static inline uint32_t crc_32r(
    const void *data, uint32_t size, uint32_t crc)
{
    const uint8_t *bytes = data;
    uint32_t index;

    for(index = 0; index < size; ++index) {
        unsigned bit;
        crc ^= bytes[index];
        for(bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^
                (0xedb88320u & (uint32_t)-(int32_t)(crc & 1u));
    }
    return crc;
}

#endif
