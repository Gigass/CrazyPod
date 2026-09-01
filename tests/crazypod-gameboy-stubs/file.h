#ifndef GAMEBOY_TEST_FILE_H
#define GAMEBOY_TEST_FILE_H
#include <stddef.h>
#include <sys/types.h>
#define O_RDONLY 0
#define O_WRONLY 1
#define O_CREAT 2
#define O_TRUNC 4
int gb_test_open(const char *path, int flags, ...);
ssize_t gb_test_read(int fd, void *data, size_t size);
ssize_t gb_test_write(int fd, const void *data, size_t size);
off_t gb_test_lseek(int fd, off_t offset, int origin);
off_t filesize(int fd);
int gb_test_close(int fd);
int gb_test_fsync(int fd);
int gb_test_rename(const char *from, const char *to);
int gb_test_remove(const char *path);
#define open gb_test_open
#define read gb_test_read
#define write gb_test_write
#define lseek gb_test_lseek
#define close gb_test_close
#define fsync gb_test_fsync
#define rename gb_test_rename
#define remove gb_test_remove
#endif
