#ifndef EPUB_HOST_TEST_FILE_H
#define EPUB_HOST_TEST_FILE_H

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static inline bool file_exists(const char *path)
{
    return access(path, F_OK) == 0;
}

static inline off_t filesize(int fd)
{
    struct stat info;
    return fstat(fd, &info) == 0 ? info.st_size : -1;
}

static inline int modtime(const char *path, time_t modification_time)
{
    (void)path;
    (void)modification_time;
    return 0;
}

#endif
