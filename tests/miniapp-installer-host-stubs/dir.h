#ifndef TEST_MINIAPP_INSTALLER_DIR_H
#define TEST_MINIAPP_INSTALLER_DIR_H

#include <stdbool.h>
#include <sys/types.h>
#include <time.h>

#include "config.h"

#define ATTR_DIRECTORY 0x10u

typedef struct test_directory DIR;

struct dirent {
    char d_name[MAX_PATH];
};

struct dirinfo {
    unsigned int attribute;
    off_t size;
    time_t mtime;
    time_t ctime;
    unsigned int ctime_tenth;
};

DIR *test_opendir(const char *path);
struct dirent *test_readdir(DIR *directory);
int test_closedir(DIR *directory);
struct dirinfo test_dir_get_info(
    DIR *directory, struct dirent *entry);
bool test_dir_exists(const char *path);
int test_mkdir(const char *path);

#define opendir test_opendir
#define readdir test_readdir
#define closedir test_closedir
#define dir_get_info test_dir_get_info
#define dir_exists test_dir_exists
#define mkdir test_mkdir

#endif
