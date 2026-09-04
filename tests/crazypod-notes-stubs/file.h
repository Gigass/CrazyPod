#ifndef CRAZYPOD_NOTES_HOST_FILE_H
#define CRAZYPOD_NOTES_HOST_FILE_H

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

extern const char *notes_test_root;
extern bool notes_test_fail_index_open;
extern bool notes_test_fail_index_rename;
extern bool notes_test_fail_index_rename_persistent;

static inline void notes_test_map_path(
    char *mapped, size_t capacity, const char *path)
{
    snprintf(mapped, capacity, "%s%s", notes_test_root, path);
}

static inline bool notes_test_suffix(
    const char *path, const char *suffix)
{
    size_t path_length = strlen(path);
    size_t suffix_length = strlen(suffix);

    return path_length >= suffix_length &&
        strcmp(path + path_length - suffix_length, suffix) == 0;
}

static inline int notes_test_open(
    const char *path, int flags, ...)
{
    char mapped[2 * MAX_PATH];
    va_list arguments;
    mode_t mode = 0;

    if(notes_test_fail_index_open && notes_test_suffix(path, "/index.tmp")) {
        errno = EIO;
        return -1;
    }
    notes_test_map_path(mapped, sizeof(mapped), path);
    if((flags & O_CREAT) != 0) {
        va_start(arguments, flags);
        mode = (mode_t)va_arg(arguments, int);
        va_end(arguments);
        return open(mapped, flags, mode);
    }
    return open(mapped, flags);
}

static inline int notes_test_mkdir(const char *path)
{
    char mapped[2 * MAX_PATH];

    notes_test_map_path(mapped, sizeof(mapped), path);
    return mkdir(mapped, 0700);
}

static inline int notes_test_remove(const char *path)
{
    char mapped[2 * MAX_PATH];

    notes_test_map_path(mapped, sizeof(mapped), path);
    return remove(mapped);
}

static inline int notes_test_rename(
    const char *old_path, const char *new_path)
{
    char mapped_old[2 * MAX_PATH];
    char mapped_new[2 * MAX_PATH];

    if(notes_test_fail_index_rename &&
       notes_test_suffix(old_path, "/index.tmp") &&
       notes_test_suffix(new_path, "/index.bin")) {
        if(!notes_test_fail_index_rename_persistent)
            notes_test_fail_index_rename = false;
        errno = EIO;
        return -1;
    }
    notes_test_map_path(mapped_old, sizeof(mapped_old), old_path);
    notes_test_map_path(mapped_new, sizeof(mapped_new), new_path);
    if(notes_test_suffix(new_path, "/index.bin"))
        remove(mapped_new);
    return rename(mapped_old, mapped_new);
}

static inline bool notes_test_file_exists(const char *path)
{
    char mapped[2 * MAX_PATH];
    struct stat status;

    notes_test_map_path(mapped, sizeof(mapped), path);
    return stat(mapped, &status) == 0;
}

static inline int notes_test_fsync(int fd)
{
    (void)fd;
    return 0;
}

#define open notes_test_open
#define mkdir notes_test_mkdir
#define remove notes_test_remove
#define rename notes_test_rename
#define file_exists notes_test_file_exists
#define fsync notes_test_fsync

#endif
