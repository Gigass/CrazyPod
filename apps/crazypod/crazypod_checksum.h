#ifndef CRAZYPOD_CHECKSUM_H
#define CRAZYPOD_CHECKSUM_H

#include <stddef.h>
#include <stdint.h>

static inline uint32_t crazypod_checksum_with_zeroed_u32(
    const void *data, size_t size, size_t checksum_offset)
{
    const unsigned char *bytes = data;
    uint32_t hash = 2166136261u;
    size_t i;

    for(i = 0; i < size; ++i) {
        unsigned char value =
            i >= checksum_offset &&
            i < checksum_offset + sizeof(uint32_t)
                ? 0 : bytes[i];

        hash ^= value;
        hash *= 16777619u;
    }
    return hash;
}

#endif
