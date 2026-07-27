#ifndef CRAZYPOD_EPUB_H
#define CRAZYPOD_EPUB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*crazypod_epub_progress_callback)(
    int percent, const char *stage, void *context);

void crazypod_epub_set_progress_callback(
    crazypod_epub_progress_callback callback, void *context);
bool crazypod_epub_probe(const char *epub_path,
                         uint32_t source_size,
                         uint32_t source_mtime,
                         char *title, size_t title_size,
                         char *author, size_t author_size,
                         char *cover_path, size_t cover_path_size);
bool crazypod_epub_prepare(const char *epub_path,
                           uint32_t source_size,
                           uint32_t source_mtime,
                           char *text_path,
                           size_t text_path_size,
                           uint32_t *text_size);
int crazypod_epub_chapter_count(void);
bool crazypod_epub_chapter_get(int index, char *title,
                               size_t title_size, uint32_t *offset);
void crazypod_epub_book_info(char *title, size_t title_size,
                             char *author, size_t author_size,
                             char *cover_path, size_t cover_path_size);
void crazypod_epub_remove_cache(const char *epub_path);

#endif
