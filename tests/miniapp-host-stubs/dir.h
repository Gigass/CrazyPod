#ifndef TEST_MINIAPP_DIR_H
#define TEST_MINIAPP_DIR_H

#include <stdbool.h>
#include <sys/stat.h>

static inline bool dir_exists(const char *path)
{
    struct stat status;

    return stat(path, &status) == 0 && S_ISDIR(status.st_mode);
}

static inline int miniapp_test_mkdir(const char *path)
{
    return mkdir(path, 0777);
}

#define mkdir(path) miniapp_test_mkdir(path)

#endif
