#ifndef TEST_MINIAPP_INSTALLER_FILE_H
#define TEST_MINIAPP_INSTALLER_FILE_H

#define O_RDONLY 0

static inline int test_open(const char *path, int flags)
{
    (void)path;
    (void)flags;
    return -1;
}

static inline int test_close(int fd)
{
    (void)fd;
    return 0;
}

#define open test_open
#define close test_close

#endif
