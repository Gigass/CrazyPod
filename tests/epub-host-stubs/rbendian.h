#ifndef EPUB_HOST_TEST_RBENDIAN_H
#define EPUB_HOST_TEST_RBENDIAN_H

#include <stdint.h>

#define htole16(value) ((uint16_t)(value))
#define htole32(value) ((uint32_t)(value))
#define letoh16(value) ((uint16_t)(value))
#define letoh32(value) ((uint32_t)(value))

#endif
