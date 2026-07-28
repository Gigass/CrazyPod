#ifndef CRAZYPOD_EPUB_CACHE_H
#define CRAZYPOD_EPUB_CACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

#ifndef EPUB_CACHE_PARENT
#define EPUB_CACHE_PARENT "/.crazypod"
#endif
#ifndef EPUB_CACHE_DIRECTORY
#define EPUB_CACHE_DIRECTORY EPUB_CACHE_PARENT "/books"
#endif

#define CRAZYPOD_EPUB_CHAPTER_MAX 128

struct crazypod_epub_cache_chapter {
    uint32_t offset;
    char title[96];
};

struct crazypod_epub_cache_book {
    uint32_t text_size;
    uint32_t chapter_count;
    char title[96];
    char author[96];
    char cover_path[MAX_PATH];
    struct crazypod_epub_cache_chapter
        chapters[CRAZYPOD_EPUB_CHAPTER_MAX];
};

struct crazypod_epub_cache_info {
    char title[96];
    char author[96];
    char cover_path[MAX_PATH];
};

struct crazypod_epub_cache_paths {
    char text[MAX_PATH];
    char metadata[MAX_PATH];
    char info[MAX_PATH];
    char temporary[MAX_PATH];
    char extract_root[MAX_PATH];
    char probe_root[MAX_PATH];
};

uint32_t crazypod_epub_cache_path_hash(const char *path);
void crazypod_epub_cache_ensure_directory(void);
void crazypod_epub_cache_paths(
    const char *source_path, struct crazypod_epub_cache_paths *paths);
bool crazypod_epub_cache_load_book(
    const char *source_path, uint32_t source_size,
    uint32_t source_mtime, const char *text_path,
    const char *metadata_path, struct crazypod_epub_cache_book *book);
bool crazypod_epub_cache_save_book(
    const char *source_path, uint32_t source_size,
    uint32_t source_mtime, const char *metadata_path,
    const struct crazypod_epub_cache_book *book);
bool crazypod_epub_cache_load_info(
    const char *source_path, uint32_t source_size,
    uint32_t source_mtime, const char *info_path,
    struct crazypod_epub_cache_info *info);
bool crazypod_epub_cache_save_info(
    const char *source_path, uint32_t source_size,
    uint32_t source_mtime, const char *info_path,
    const struct crazypod_epub_cache_info *info);
void crazypod_epub_cache_remove(const char *source_path);

#endif
