#ifndef CRAZYPOD_EPUB_COVER_STORE_H
#define CRAZYPOD_EPUB_COVER_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool crazypod_epub_cover_is_image(const char *path);
bool crazypod_epub_cover_resolve_document(
    const char *root, char *path, size_t path_size);
bool crazypod_epub_cover_persist(
    uint32_t hash, const char *root, const char *cover_source,
    char *destination, size_t destination_size);

#endif
