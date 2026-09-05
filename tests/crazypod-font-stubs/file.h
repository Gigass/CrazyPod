#ifndef CRAZYPOD_FONT_TEST_FILE_H
#define CRAZYPOD_FONT_TEST_FILE_H
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
int test_font_open(const char *path, int flags);
int test_font_close(int fd);
ssize_t test_font_read(int fd, void *buffer, size_t size);
off_t test_font_lseek(int fd, off_t offset, int whence);
int filesize(int fd);
#define open test_font_open
#define close test_font_close
#define read test_font_read
#define lseek test_font_lseek
#endif
