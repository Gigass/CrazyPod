#ifndef EPUB_HOST_TEST_ZIP_H
#define EPUB_HOST_TEST_ZIP_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

struct zip;

enum {
    ZIP_PASS_SHALLOW,
    ZIP_PASS_START,
    ZIP_PASS_DATA,
    ZIP_PASS_END,
};

struct zip_args {
    uint16_t entry;
    uint16_t entries;
    char *name;
    uint32_t file_size;
    time_t mtime;
    void *block;
    uint32_t block_size;
    uint32_t read_size;
};

typedef int (*zip_callback)(
    const struct zip_args *args, int pass, void *ctx);

struct zip *zip_open(const char *path, bool memory);
int zip_read_shallow(struct zip *archive,
                     zip_callback callback, void *context);
int zip_read_deep(struct zip *archive,
                  zip_callback callback, void *context);
int zip_extract(struct zip *archive, const char *root,
                zip_callback callback, void *context);
void zip_close(struct zip *archive);

#endif
