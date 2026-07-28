#ifndef CRAZYPOD_EPUB_EXTRACTION_H
#define CRAZYPOD_EPUB_EXTRACTION_H

#include <stdbool.h>
#include <stddef.h>

#include "config.h"

bool crazypod_epub_extraction_remove_tree(const char *path);
bool crazypod_epub_extraction_archive_name(
    char *output, size_t size, const char *root, const char *path);
bool crazypod_epub_extraction_entries(
    const char *epub_path, const char *extract_root,
    const char entries[][MAX_PATH], int entry_count);
bool crazypod_epub_extraction_entry(
    const char *epub_path, const char *extract_root, const char *entry);

#endif
