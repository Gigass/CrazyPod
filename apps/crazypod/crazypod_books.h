#ifndef CRAZYPOD_BOOKS_H
#define CRAZYPOD_BOOKS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "epub/crazypod_epub_layout.h"

#include "file.h"

#define CRAZYPOD_BOOKMARK_NONE UINT32_MAX

enum crazypod_book_format {
    CRAZYPOD_BOOK_TXT,
    CRAZYPOD_BOOK_MARKDOWN,
    CRAZYPOD_BOOK_EPUB,
};

struct crazypod_book {
    char path[MAX_PATH];
    char title[96];
    char author[96];
    char cover_path[MAX_PATH];
    enum crazypod_book_format format;
    uint32_t size;
    uint32_t content_size;
    uint32_t mtime;
    uint32_t progress;
    uint32_t bookmark;
    bool favorite;
    bool details_loaded;
};

typedef void (*crazypod_book_progress_callback)(
    int percent, const char *stage, void *context);

void crazypod_books_init(void);
void crazypod_books_scan(void);
bool crazypod_books_scan_needed(void);
void crazypod_books_invalidate_scan(void);
int crazypod_books_count(void);
const struct crazypod_book *crazypod_book_get(int index);
int crazypod_book_index(const struct crazypod_book *book);
int crazypod_books_recent_index(void);
int crazypod_books_recent_count(void);
int crazypod_books_recent_at(int position);
int crazypod_books_favorite_count(void);
int crazypod_books_favorite_at(int position);
bool crazypod_book_read_page(int index, uint32_t offset,
                             char *text, size_t size,
                             uint32_t *next_offset);
bool crazypod_book_page_image(int index, uint32_t offset,
                              char *path, size_t path_size);
bool crazypod_book_set_progress(int index, uint32_t offset);
bool crazypod_book_toggle_bookmark(int index, uint32_t offset);
bool crazypod_book_toggle_favorite(int index);
bool crazypod_book_probe(int index);
bool crazypod_book_prepare(int index);
bool crazypod_book_prepare_with_progress(
    int index, crazypod_book_progress_callback callback, void *context);
int crazypod_book_chapter_count(int index);
bool crazypod_book_chapter_get(int index, int chapter,
                               char *title, size_t title_size,
                               uint32_t *offset);
bool crazypod_book_delete(int index);
int crazypod_books_font_size(void);
int crazypod_books_theme(void);
bool crazypod_books_set_font_size(int value);
bool crazypod_books_set_theme(int value);
/* Keep pagination in sync with the reader's currently visible viewport. */
void crazypod_books_set_reader_layout(
    unsigned max_lines, unsigned max_line_width,
    crazypod_epub_layout_width_fn measure_width, void *context);

#endif
