#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "dir.h"
#include "crazypod_books.h"
#include "crazypod_epub.h"

#define CATALOG_PATH "/.crazypod/books/catalog.bin"
#define CATALOG_TEMP "/.crazypod/books/catalog.tmp"

struct test_file {
    unsigned char data[2048];
    size_t size;
    bool exists;
};

struct open_file {
    struct test_file *file;
    size_t offset;
};

struct crazypod_books_test_directory {
    int next;
};

static struct test_file catalog;
static struct test_file temporary;
static struct open_file handles[4];
static struct crazypod_books_test_directory books_directory;
static struct DIRENT book_entry;
static int directory_open_count;

static struct test_file *test_file_at(const char *path)
{
    if(strcmp(path, CATALOG_PATH) == 0)
        return &catalog;
    if(strcmp(path, CATALOG_TEMP) == 0)
        return &temporary;
    return NULL;
}

int books_test_open(const char *path, int flags, ...)
{
    struct test_file *file = test_file_at(path);
    int i;

    (void)flags;
    if(file == NULL || (!file->exists && !(flags & O_CREAT)))
        return -1;
    if(flags & O_CREAT)
        file->exists = true;
    if(flags & O_TRUNC)
        file->size = 0;
    for(i = 0; i < 4; ++i) {
        if(handles[i].file == NULL) {
            handles[i].file = file;
            handles[i].offset = 0;
            return i;
        }
    }
    return -1;
}

ssize_t books_test_read(int fd, void *data, size_t size)
{
    struct open_file *handle = &handles[fd];
    size_t available = handle->file->size - handle->offset;

    if(size > available)
        size = available;
    memcpy(data, handle->file->data + handle->offset, size);
    handle->offset += size;
    return (ssize_t)size;
}

ssize_t books_test_write(int fd, const void *data, size_t size)
{
    struct open_file *handle = &handles[fd];

    assert(handle->offset + size <= sizeof(handle->file->data));
    memcpy(handle->file->data + handle->offset, data, size);
    handle->offset += size;
    if(handle->file->size < handle->offset)
        handle->file->size = handle->offset;
    return (ssize_t)size;
}

off_t books_test_lseek(int fd, off_t offset, int origin)
{
    (void)origin;
    handles[fd].offset = (size_t)offset;
    return offset;
}

off_t filesize(int fd)
{
    return (off_t)handles[fd].file->size;
}

int books_test_close(int fd)
{
    handles[fd].file = NULL;
    handles[fd].offset = 0;
    return 0;
}

int books_test_fsync(int fd)
{
    (void)fd;
    return 0;
}

int books_test_rename(const char *from, const char *to)
{
    struct test_file *source = test_file_at(from);
    struct test_file *target = test_file_at(to);

    if(source == NULL || target == NULL || !source->exists)
        return -1;
    *target = *source;
    memset(source, 0, sizeof(*source));
    return 0;
}

int books_test_remove(const char *path)
{
    struct test_file *file = test_file_at(path);

    if(file == NULL || !file->exists)
        return -1;
    memset(file, 0, sizeof(*file));
    return 0;
}

DIR *opendir(const char *path)
{
    if(strcmp(path, "/Books") != 0)
        return NULL;
    ++directory_open_count;
    books_directory.next = 0;
    return &books_directory;
}

struct DIRENT *readdir(DIR *directory)
{
    if(directory->next++ != 0)
        return NULL;
    snprintf(book_entry.d_name, sizeof(book_entry.d_name), "Novel.epub");
    return &book_entry;
}

struct dirinfo dir_get_info(DIR *directory, struct DIRENT *entry)
{
    struct dirinfo info = { 1234, 5678, 0 };

    (void)directory;
    (void)entry;
    return info;
}

int closedir(DIR *directory)
{
    (void)directory;
    return 0;
}

int mkdir(const char *path)
{
    (void)path;
    return 0;
}

unsigned char *iso_decode_ex(
    const unsigned char *source, unsigned char *target,
    int codepage, int count, int target_size)
{
    (void)source;
    (void)codepage;
    (void)count;
    (void)target_size;
    return target;
}

unsigned char *utf16decode(
    const unsigned char *source, unsigned char *target,
    int count, int target_size, bool le)
{
    (void)source;
    (void)count;
    (void)target_size;
    (void)le;
    return target;
}

void crazypod_epub_set_progress_callback(
    crazypod_epub_progress_callback callback, void *context)
{
    (void)callback;
    (void)context;
}

bool crazypod_epub_probe(
    const char *path, uint32_t size, uint32_t mtime,
    char *title, size_t title_size,
    char *author, size_t author_size,
    char *cover, size_t cover_size)
{
    (void)path;
    (void)size;
    (void)mtime;
    (void)title;
    (void)title_size;
    (void)author;
    (void)author_size;
    (void)cover;
    (void)cover_size;
    return false;
}

bool crazypod_epub_prepare(
    const char *path, uint32_t size, uint32_t mtime,
    char *text_path, size_t text_path_size, uint32_t *text_size)
{
    (void)path;
    (void)size;
    (void)mtime;
    (void)text_path;
    (void)text_path_size;
    if(text_path_size > 0)
        snprintf(text_path, text_path_size, "/cache/book.txt");
    if(text_size != NULL)
        *text_size = 900;
    return true;
}

bool crazypod_epub_image_get(
    uint32_t offset, char *path, size_t path_size)
{
    (void)offset;
    if(path != NULL && path_size > 0)
        path[0] = '\0';
    return false;
}

void crazypod_epub_text_path(
    const char *path, char *text_path, size_t text_path_size)
{
    (void)path;
    if(text_path_size > 0)
        text_path[0] = '\0';
}

int crazypod_epub_chapter_count(void)
{
    return 0;
}

bool crazypod_epub_chapter_get(
    int index, char *title, size_t title_size, uint32_t *offset)
{
    (void)index;
    (void)title;
    (void)title_size;
    (void)offset;
    return false;
}

void crazypod_epub_book_info(
    char *title, size_t title_size,
    char *author, size_t author_size,
    char *cover, size_t cover_size)
{
    if(title_size > 0)
        snprintf(title, title_size, "Cached Novel");
    if(author_size > 0)
        snprintf(author, author_size, "Author");
    if(cover_size > 0)
        cover[0] = '\0';
}

void crazypod_epub_remove_cache(const char *path)
{
    (void)path;
}

int main(void)
{
    const struct crazypod_book *book;

    crazypod_books_init();
    assert(crazypod_books_scan_needed());
    crazypod_books_scan();
    assert(catalog.exists);
    assert(crazypod_books_count() == 1);
    assert(directory_open_count == 1);
    assert(crazypod_book_prepare(0));
    assert(crazypod_book_get(0)->content_size == 900);

    directory_open_count = 0;
    crazypod_books_init();
    assert(!crazypod_books_scan_needed());
    assert(crazypod_books_count() == 1);
    assert(directory_open_count == 0);
    book = crazypod_book_get(0);
    assert(book != NULL);
    assert(strcmp(book->path, "/Books/Novel.epub") == 0);
    assert(book->size == 1234 && book->mtime == 5678);
    assert(book->content_size == 900);

    catalog.data[0] ^= 1;
    crazypod_books_init();
    assert(crazypod_books_scan_needed());
    assert(crazypod_books_count() == 0);
    crazypod_books_scan();

    crazypod_books_invalidate_scan();
    assert(crazypod_books_scan_needed());
    assert(!catalog.exists);
    crazypod_books_init();
    assert(crazypod_books_scan_needed());
    assert(crazypod_books_count() == 0);

    puts("Books catalog: cold-boot reuse and USB invalidation pass");
    return 0;
}
