#ifndef EPUB_HOST_TEST_TIMEFUNCS_H
#define EPUB_HOST_TEST_TIMEFUNCS_H

#include <stdint.h>
#include <time.h>

static inline time_t dostime_mktime(uint16_t date, uint16_t time)
{
    (void)date;
    (void)time;
    return 0;
}

#endif
