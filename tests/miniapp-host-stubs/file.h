#ifndef TEST_MINIAPP_FILE_H
#define TEST_MINIAPP_FILE_H

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static inline off_t filesize(int fd)
{
    struct stat status;
    return fstat(fd, &status) == 0 ? status.st_size : (off_t)-1;
}

#endif
