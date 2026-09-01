#ifndef CRAZYPOD_BOOKS_TEST_RBUNICODE_H
#define CRAZYPOD_BOOKS_TEST_RBUNICODE_H

#define GB_2312 0

unsigned char *iso_decode_ex(
    const unsigned char *source, unsigned char *target,
    int codepage, int count, int target_size);

#endif
