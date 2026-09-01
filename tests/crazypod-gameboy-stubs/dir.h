#ifndef GAMEBOY_TEST_DIR_H
#define GAMEBOY_TEST_DIR_H
#include <stdbool.h>
typedef struct { const char *path; int next; } DIR;
struct dirent { char d_name[260]; };
struct dirinfo { int attribute; };
#define ATTR_DIRECTORY 16
DIR *opendir(const char *path);
struct dirent *readdir(DIR *directory);
struct dirinfo dir_get_info(DIR *directory, struct dirent *entry);
int closedir(DIR *directory);
bool dir_exists(const char *path);
int mkdir(const char *path);
#endif
