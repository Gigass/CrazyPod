#ifndef CRAZYPOD_BOOKS_TEST_FILE_H
#define CRAZYPOD_BOOKS_TEST_FILE_H

#include <stddef.h>
#include <sys/types.h>

#define O_RDONLY 0
#define O_WRONLY 1
#define O_CREAT 2
#define O_TRUNC 4

int books_test_open(const char *path, int flags, ...);
ssize_t books_test_read(int fd, void *data, size_t size);
ssize_t books_test_write(int fd, const void *data, size_t size);
off_t books_test_lseek(int fd, off_t offset, int origin);
off_t filesize(int fd);
int books_test_close(int fd);
int books_test_fsync(int fd);
int books_test_rename(const char *from, const char *to);
int books_test_remove(const char *path);

#define open books_test_open
#define read books_test_read
#define write books_test_write
#define lseek books_test_lseek
#define close books_test_close
#define fsync books_test_fsync
#define rename books_test_rename
#define remove books_test_remove

#endif
