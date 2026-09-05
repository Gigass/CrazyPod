#ifndef CRAZYPOD_BOOKS_TEST_PANIC_H
#define CRAZYPOD_BOOKS_TEST_PANIC_H

#include <stdarg.h>
#include <stdlib.h>

static inline void panicf(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    va_end(args);
    abort();
}

#endif
