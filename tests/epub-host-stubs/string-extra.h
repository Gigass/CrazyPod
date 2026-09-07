#ifndef EPUB_HOST_TEST_STRING_EXTRA_H
#define EPUB_HOST_TEST_STRING_EXTRA_H

#include <string.h>

static inline size_t strlcpy(
    char *destination, const char *source, size_t size)
{
    size_t length = strlen(source);

    if(size > 0) {
        size_t copy = length < size - 1 ? length : size - 1;

        memcpy(destination, source, copy);
        destination[copy] = '\0';
    }
    return length;
}

#endif
