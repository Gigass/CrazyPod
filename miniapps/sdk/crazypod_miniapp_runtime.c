/*
 * Minimal freestanding memory primitives for native CrazyPod payloads.
 * Mini-apps are linked as raw images and therefore cannot import libc.
 */

#include <stddef.h>

void *memcpy(void *destination, const void *source, size_t size)
{
    unsigned char *output = destination;
    const unsigned char *input = source;
    size_t index;

    for(index = 0; index < size; ++index)
        output[index] = input[index];
    return destination;
}

void *memmove(void *destination, const void *source, size_t size)
{
    unsigned char *output = destination;
    const unsigned char *input = source;

    if(output < input) {
        size_t index;

        for(index = 0; index < size; ++index)
            output[index] = input[index];
    }
    else if(output > input) {
        while(size > 0) {
            --size;
            output[size] = input[size];
        }
    }
    return destination;
}

void *memset(void *destination, int value, size_t size)
{
    unsigned char *output = destination;
    size_t index;

    for(index = 0; index < size; ++index)
        output[index] = (unsigned char)value;
    return destination;
}

int memcmp(const void *left, const void *right, size_t size)
{
    const unsigned char *first = left;
    const unsigned char *second = right;
    size_t index;

    for(index = 0; index < size; ++index) {
        if(first[index] != second[index])
            return first[index] < second[index] ? -1 : 1;
    }
    return 0;
}
