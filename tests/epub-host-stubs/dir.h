#ifndef EPUB_HOST_TEST_DIR_H
#define EPUB_HOST_TEST_DIR_H

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#define DIRENT dirent
#define ATTR_DIRECTORY 1

struct dirinfo {
    off_t size;
    time_t mtime;
    int attribute;
};

static inline struct dirinfo dir_get_info(
    DIR *directory, const struct dirent *entry)
{
    struct dirinfo result = {0, 0, 0};
    (void)directory;
#ifdef DT_DIR
    result.attribute =
        entry->d_type == DT_DIR ? ATTR_DIRECTORY : 0;
#else
    (void)entry;
#endif
    return result;
}

static inline int host_test_mkdir(const char *path)
{
    return mkdir(path, 0777);
}

static inline int dir_exists(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

#define mkdir(path) host_test_mkdir(path)

#endif
