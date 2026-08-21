#ifndef TEST_SYSTEM_H
#define TEST_SYSTEM_H

#include <stddef.h>
#include <stdint.h>

/* Rockbox targets use an int-sized buflib word. Host tests use intptr_t so
 * use a width-preserving absolute-value helper instead of libc abs(int). */
#undef abs
#define abs(value) ((value) < 0 ? -(value) : (value))

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define ALIGN_DOWN(n, a) \
    ((typeof(n))((uintptr_t)(n) / (a) * (a)))
#define ALIGN_UP(n, a) ALIGN_DOWN((n) + ((a) - 1), (a))
#define ALIGN_BUFFER(ptr, size, align) \
({ \
    size_t test_size = (size); \
    size_t test_align = (align); \
    uintptr_t test_start = (uintptr_t)(ptr); \
    uintptr_t test_end = test_start + test_size; \
    test_start = ALIGN_UP(test_start, test_align); \
    test_end = ALIGN_DOWN(test_end, test_align); \
    (ptr) = (typeof(ptr))test_start; \
    (size) = test_end > test_start ? test_end - test_start : 0; \
})
#define IS_ALIGNED(value, align) \
    (((value) & ((typeof(value))(align) - 1)) == 0)
#define alignof __alignof__

#endif
