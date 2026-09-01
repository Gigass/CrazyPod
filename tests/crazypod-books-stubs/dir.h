#ifndef CRAZYPOD_BOOKS_TEST_DIR_H
#define CRAZYPOD_BOOKS_TEST_DIR_H

#include <stdbool.h>
#include <sys/types.h>
#include <time.h>

#define ATTR_DIRECTORY 16

typedef struct crazypod_books_test_directory DIR;

struct DIRENT {
    char d_name[260];
};

struct dirinfo {
    off_t size;
    time_t mtime;
    int attribute;
};

DIR *opendir(const char *path);
struct DIRENT *readdir(DIR *directory);
struct dirinfo dir_get_info(DIR *directory, struct DIRENT *entry);
int closedir(DIR *directory);
int mkdir(const char *path);

#endif
