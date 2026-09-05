#include <string.h>
#undef strlcpy
#define strlcpy test_strlcpy
static inline size_t test_strlcpy(char *dst, const char *src, size_t size)
{
    size_t length = strlen(src);
    if(size != 0) {
        size_t count = length < size - 1 ? length : size - 1;
        memcpy(dst, src, count);
        dst[count] = '\0';
    }
    return length;
}
