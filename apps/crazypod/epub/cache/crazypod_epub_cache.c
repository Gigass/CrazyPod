#include "config.h"

#ifdef IPOD_6G

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dir.h"
#include "file.h"

#include "crazypod_epub_cache.h"

#define EPUB_CACHE_MAGIC 0x43504550u
#define EPUB_INFO_MAGIC 0x43504549u
#define EPUB_CACHE_VERSION 7u
#define EPUB_CACHE_VERSION_LEGACY 6u

struct epub_cache_disk {
    uint32_t magic;
    uint32_t version;
    uint32_t source_size;
    uint32_t source_mtime;
    char source_path[MAX_PATH];
    uint32_t text_size;
    uint32_t chapter_count;
    uint32_t image_count;
    char title[96];
    char author[96];
    char cover_path[MAX_PATH];
    struct crazypod_epub_cache_chapter
        chapters[CRAZYPOD_EPUB_CHAPTER_MAX];
    struct crazypod_epub_cache_image
        images[CRAZYPOD_EPUB_IMAGE_MAX];
    uint32_t checksum;
};

struct epub_info_disk {
    uint32_t magic;
    uint32_t version;
    uint32_t source_size;
    uint32_t source_mtime;
    char source_path[MAX_PATH];
    char title[96];
    char author[96];
    char cover_path[MAX_PATH];
    uint32_t checksum;
};

static struct epub_cache_disk cache_disk;
static struct epub_info_disk info_disk;

static uint32_t hash_bytes(uint32_t hash, const void *data, size_t size)
{
    const unsigned char *bytes = data;
    size_t i;

    for(i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t cache_checksum(const struct epub_cache_disk *cache)
{
    return hash_bytes(
        2166136261u, cache,
        offsetof(struct epub_cache_disk, checksum));
}

static uint32_t info_checksum(const struct epub_info_disk *info)
{
    return hash_bytes(
        2166136261u, info,
        offsetof(struct epub_info_disk, checksum));
}

static bool cache_version_valid(uint32_t version)
{
    return version == EPUB_CACHE_VERSION ||
        version == EPUB_CACHE_VERSION_LEGACY;
}

static bool cache_string_terminated(const char *value, size_t size)
{
    return memchr(value, '\0', size) != NULL;
}

static bool cache_text_valid(const char *path)
{
    unsigned char buffer[256];
    int fd = open(path, O_RDONLY);
    ssize_t count;

    if(fd < 0)
        return false;
    while((count = read(fd, buffer, sizeof(buffer))) > 0) {
        if(memchr(buffer, '\0', (size_t)count) != NULL) {
            close(fd);
            return false;
        }
    }
    close(fd);
    return count == 0;
}

static bool cache_text_size_valid(const char *path, uint32_t expected)
{
    int fd = open(path, O_RDONLY);
    off_t size;

    if(fd < 0)
        return false;
    size = filesize(fd);
    close(fd);
    return size >= 0 && (uint32_t)size == expected;
}

static bool read_exact(int fd, void *buffer, size_t size)
{
    unsigned char *cursor = buffer;

    while(size > 0) {
        ssize_t count = read(fd, cursor, size);
        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static bool write_exact(int fd, const void *buffer, size_t size)
{
    const unsigned char *cursor = buffer;

    while(size > 0) {
        ssize_t count = write(fd, cursor, size);
        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static bool save_record(
    const char *path, const void *record, size_t record_size)
{
    char temporary[MAX_PATH];
    int fd;
    bool success;

    if(snprintf(temporary, sizeof(temporary), "%s.tmp", path) >=
       (int)sizeof(temporary))
        return false;
    fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    success = write_exact(fd, record, record_size) && fsync(fd) >= 0;
    close(fd);
    if(!success || rename(temporary, path) < 0) {
        remove(temporary);
        return false;
    }
    return true;
}

uint32_t crazypod_epub_cache_path_hash(const char *path)
{
    return hash_bytes(2166136261u, path, strlen(path));
}

void crazypod_epub_cache_ensure_directory(void)
{
    mkdir(EPUB_CACHE_PARENT);
    mkdir(EPUB_CACHE_DIRECTORY);
}

void crazypod_epub_cache_paths(
    const char *source_path, struct crazypod_epub_cache_paths *paths)
{
    unsigned long hash =
        (unsigned long)crazypod_epub_cache_path_hash(source_path);

    snprintf(paths->text, sizeof(paths->text),
             EPUB_CACHE_DIRECTORY "/%08lx.txt", hash);
    snprintf(paths->metadata, sizeof(paths->metadata),
             EPUB_CACHE_DIRECTORY "/%08lx.meta", hash);
    snprintf(paths->info, sizeof(paths->info),
             EPUB_CACHE_DIRECTORY "/%08lx.info", hash);
    snprintf(paths->temporary, sizeof(paths->temporary),
             EPUB_CACHE_DIRECTORY "/%08lx.tmp", hash);
    snprintf(paths->extract_root, sizeof(paths->extract_root),
             EPUB_CACHE_DIRECTORY "/%08lx.epub", hash);
    snprintf(paths->probe_root, sizeof(paths->probe_root),
             EPUB_CACHE_DIRECTORY "/%08lx.probe", hash);
}

bool crazypod_epub_cache_load_book(
    const char *source_path, uint32_t source_size,
    uint32_t source_mtime, const char *text_path,
    const char *metadata_path, struct crazypod_epub_cache_book *book)
{
    int fd = open(metadata_path, O_RDONLY);
    bool valid;

    if(fd < 0)
        return false;
    valid = read_exact(fd, &cache_disk, sizeof(cache_disk)) &&
        cache_disk.magic == EPUB_CACHE_MAGIC &&
        cache_version_valid(cache_disk.version) &&
        cache_disk.source_size == source_size &&
        cache_disk.source_mtime == source_mtime &&
        cache_string_terminated(
            cache_disk.source_path, sizeof(cache_disk.source_path)) &&
        strcmp(cache_disk.source_path, source_path) == 0 &&
        cache_disk.chapter_count <= CRAZYPOD_EPUB_CHAPTER_MAX &&
        cache_disk.image_count <= CRAZYPOD_EPUB_IMAGE_MAX &&
        cache_disk.checksum == cache_checksum(&cache_disk) &&
        cache_string_terminated(cache_disk.title, sizeof(cache_disk.title)) &&
        cache_string_terminated(
            cache_disk.author, sizeof(cache_disk.author)) &&
        cache_string_terminated(
            cache_disk.cover_path, sizeof(cache_disk.cover_path)) &&
        file_exists(text_path) &&
        cache_text_size_valid(text_path, cache_disk.text_size) &&
        (cache_disk.version != EPUB_CACHE_VERSION_LEGACY ||
         cache_text_valid(text_path));
    if(valid) {
        uint32_t i;

        for(i = 0; i < cache_disk.image_count; ++i) {
            if(!cache_string_terminated(
                   cache_disk.images[i].path,
                   sizeof(cache_disk.images[i].path)) ||
               cache_disk.images[i].path[0] == '\0' ||
               !file_exists(cache_disk.images[i].path)) {
                valid = false;
                break;
            }
        }
    }
    close(fd);
    if(!valid)
        return false;

    memset(book, 0, sizeof(*book));
    book->text_size = cache_disk.text_size;
    book->chapter_count = cache_disk.chapter_count;
    book->image_count = cache_disk.image_count;
    snprintf(book->title, sizeof(book->title), "%s", cache_disk.title);
    snprintf(book->author, sizeof(book->author), "%s", cache_disk.author);
    snprintf(book->cover_path, sizeof(book->cover_path),
             "%s", cache_disk.cover_path);
    memcpy(book->chapters, cache_disk.chapters, sizeof(book->chapters));
    memcpy(book->images, cache_disk.images, sizeof(book->images));
    return true;
}

bool crazypod_epub_cache_save_book(
    const char *source_path, uint32_t source_size,
    uint32_t source_mtime, const char *metadata_path,
    const struct crazypod_epub_cache_book *book)
{
    if(book->chapter_count > CRAZYPOD_EPUB_CHAPTER_MAX)
        return false;
    if(book->image_count > CRAZYPOD_EPUB_IMAGE_MAX)
        return false;
    memset(&cache_disk, 0, sizeof(cache_disk));
    cache_disk.magic = EPUB_CACHE_MAGIC;
    cache_disk.version = EPUB_CACHE_VERSION;
    cache_disk.source_size = source_size;
    cache_disk.source_mtime = source_mtime;
    snprintf(cache_disk.source_path, sizeof(cache_disk.source_path),
             "%s", source_path);
    cache_disk.text_size = book->text_size;
    cache_disk.chapter_count = book->chapter_count;
    cache_disk.image_count = book->image_count;
    snprintf(cache_disk.title, sizeof(cache_disk.title), "%s", book->title);
    snprintf(cache_disk.author, sizeof(cache_disk.author), "%s", book->author);
    snprintf(cache_disk.cover_path, sizeof(cache_disk.cover_path),
             "%s", book->cover_path);
    memcpy(cache_disk.chapters, book->chapters, sizeof(cache_disk.chapters));
    memcpy(cache_disk.images, book->images, sizeof(cache_disk.images));
    cache_disk.checksum = cache_checksum(&cache_disk);
    return save_record(metadata_path, &cache_disk, sizeof(cache_disk));
}

bool crazypod_epub_cache_load_info(
    const char *source_path, uint32_t source_size,
    uint32_t source_mtime, const char *info_path,
    struct crazypod_epub_cache_info *info)
{
    int fd = open(info_path, O_RDONLY);
    bool valid;

    if(fd < 0)
        return false;
    valid = read_exact(fd, &info_disk, sizeof(info_disk)) &&
        info_disk.magic == EPUB_INFO_MAGIC &&
        cache_version_valid(info_disk.version) &&
        info_disk.source_size == source_size &&
        info_disk.source_mtime == source_mtime &&
        cache_string_terminated(
            info_disk.source_path, sizeof(info_disk.source_path)) &&
        strcmp(info_disk.source_path, source_path) == 0 &&
        info_disk.checksum == info_checksum(&info_disk) &&
        cache_string_terminated(info_disk.title, sizeof(info_disk.title)) &&
        cache_string_terminated(info_disk.author, sizeof(info_disk.author)) &&
        cache_string_terminated(
            info_disk.cover_path, sizeof(info_disk.cover_path)) &&
        (info_disk.cover_path[0] == '\0' ||
         file_exists(info_disk.cover_path));
    close(fd);
    if(!valid)
        return false;
    snprintf(info->title, sizeof(info->title), "%s", info_disk.title);
    snprintf(info->author, sizeof(info->author), "%s", info_disk.author);
    snprintf(info->cover_path, sizeof(info->cover_path),
             "%s", info_disk.cover_path);
    return true;
}

bool crazypod_epub_cache_save_info(
    const char *source_path, uint32_t source_size,
    uint32_t source_mtime, const char *info_path,
    const struct crazypod_epub_cache_info *info)
{
    memset(&info_disk, 0, sizeof(info_disk));
    info_disk.magic = EPUB_INFO_MAGIC;
    info_disk.version = EPUB_CACHE_VERSION;
    info_disk.source_size = source_size;
    info_disk.source_mtime = source_mtime;
    snprintf(info_disk.source_path, sizeof(info_disk.source_path),
             "%s", source_path);
    snprintf(info_disk.title, sizeof(info_disk.title), "%s", info->title);
    snprintf(info_disk.author, sizeof(info_disk.author), "%s", info->author);
    snprintf(info_disk.cover_path, sizeof(info_disk.cover_path),
             "%s", info->cover_path);
    info_disk.checksum = info_checksum(&info_disk);
    return save_record(info_path, &info_disk, sizeof(info_disk));
}

void crazypod_epub_cache_remove(const char *source_path)
{
    static const char *const suffixes[] = {
        ".txt", ".meta", ".info", ".tmp", ".cover.tmp",
        ".cover.jpg", ".cover.jpeg", ".cover.bmp", ".cover.png"
    };
    uint32_t hash;
    char path[MAX_PATH];
    size_t i;

    if(source_path == NULL)
        return;
    hash = crazypod_epub_cache_path_hash(source_path);
    for(i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); ++i) {
        snprintf(path, sizeof(path),
                 EPUB_CACHE_DIRECTORY "/%08lx%s",
                 (unsigned long)hash, suffixes[i]);
        remove(path);
    }
    for(i = 0; i < CRAZYPOD_EPUB_IMAGE_MAX; ++i) {
        static const char *const image_suffixes[] = {
            ".jpg", ".jpeg", ".bmp", ".png", ".gif", ".webp"
        };
        size_t suffix;

        for(suffix = 0; suffix < sizeof(image_suffixes) /
                              sizeof(image_suffixes[0]); ++suffix) {
            snprintf(path, sizeof(path),
                     EPUB_CACHE_DIRECTORY "/%08lx.image%03lu%s",
                     (unsigned long)hash, (unsigned long)i,
                     image_suffixes[suffix]);
            remove(path);
            snprintf(path, sizeof(path),
                     EPUB_CACHE_DIRECTORY "/%08lx.image%03lu%s.tmp",
                     (unsigned long)hash, (unsigned long)i,
                     image_suffixes[suffix]);
            remove(path);
        }
    }
}

#endif
