#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "file.h"

#include "../crazypod_epub_parser.h"
#include "crazypod_epub_cache.h"
#include "crazypod_epub_cover_store.h"

#define COVER_XML_SIZE 65536

static char cover_xml[COVER_XML_SIZE];
static char copy_buffer[4096];

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

static bool read_text_file(const char *path)
{
    int fd = open(path, O_RDONLY);
    ssize_t count;

    if(fd < 0)
        return false;
    count = read(fd, cover_xml, sizeof(cover_xml) - 1);
    close(fd);
    if(count <= 0)
        return false;
    cover_xml[count] = '\0';
    return true;
}

static bool copy_file(const char *source, const char *destination)
{
    int input = -1;
    int output = -1;
    ssize_t count;
    bool success = false;

    input = open(source, O_RDONLY);
    if(input < 0)
        goto done;
    output = open(destination, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(output < 0)
        goto done;
    while((count = read(input, copy_buffer, sizeof(copy_buffer))) > 0) {
        if(!write_exact(output, copy_buffer, (size_t)count))
            goto done;
    }
    if(count < 0 || fsync(output) < 0)
        goto done;
    success = true;

done:
    if(output >= 0)
        close(output);
    if(input >= 0)
        close(input);
    if(!success)
        remove(destination);
    return success;
}

static const char *path_extension(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *dot = strrchr(path, '.');

    return dot != NULL && (slash == NULL || dot > slash) ? dot : "";
}

bool crazypod_epub_cover_is_image(const char *path)
{
    const char *extension = path_extension(path);

    return crazypod_epub_ascii_equal(extension, ".jpg") ||
        crazypod_epub_ascii_equal(extension, ".jpeg") ||
        crazypod_epub_ascii_equal(extension, ".bmp") ||
        crazypod_epub_ascii_equal(extension, ".png");
}

bool crazypod_epub_cover_resolve_document(
    const char *root, char *path, size_t path_size)
{
    static char directory[MAX_PATH];
    static char href[192];
    const char *tag;
    const char *end;

    if(!read_text_file(path))
        return false;
    tag = crazypod_epub_find_start_tag(cover_xml, "img");
    if(tag == NULL)
        tag = crazypod_epub_find_start_tag(cover_xml, "image");
    if(tag == NULL || (end = strchr(tag, '>')) == NULL)
        return false;
    href[0] = '\0';
    if(!crazypod_epub_extract_attribute(
           tag, end, "src", href, sizeof(href)) &&
       !crazypod_epub_extract_attribute(
           tag, end, "href", href, sizeof(href)) &&
       !crazypod_epub_extract_attribute(
           tag, end, "xlink:href", href, sizeof(href)))
        return false;
    crazypod_epub_directory_of(directory, sizeof(directory), path);
    return crazypod_epub_resolve_href(
        path, path_size, root, directory, href);
}

bool crazypod_epub_cover_persist(
    uint32_t hash, const char *root, const char *cover_source,
    char *destination, size_t destination_size)
{
    static char source[MAX_PATH];
    static char temporary[MAX_PATH];
    static char old_path[MAX_PATH];
    static char extension[8];
    static const char *const known_extensions[] = {
        ".jpg", ".jpeg", ".bmp", ".png"
    };
    const char *source_extension;
    size_t i;

    destination[0] = '\0';
    if(cover_source == NULL || cover_source[0] == '\0')
        return false;
    snprintf(source, sizeof(source), "%s", cover_source);
    if(!crazypod_epub_cover_is_image(source) &&
       !crazypod_epub_cover_resolve_document(
           root, source, sizeof(source)))
        return false;
    source_extension = path_extension(source);
    if(strlen(source_extension) >= sizeof(extension))
        return false;
    for(i = 0; source_extension[i] != '\0'; ++i)
        extension[i] = (char)crazypod_epub_ascii_lower(
            (unsigned char)source_extension[i]);
    extension[i] = '\0';
    if(!crazypod_epub_cover_is_image(extension))
        return false;

    for(i = 0; i < sizeof(known_extensions) /
                    sizeof(known_extensions[0]); ++i) {
        snprintf(old_path, sizeof(old_path),
                 EPUB_CACHE_DIRECTORY "/%08lx.cover%s",
                 (unsigned long)hash, known_extensions[i]);
        remove(old_path);
    }
    snprintf(destination, destination_size,
             EPUB_CACHE_DIRECTORY "/%08lx.cover%s",
             (unsigned long)hash, extension);
    snprintf(temporary, sizeof(temporary),
             EPUB_CACHE_DIRECTORY "/%08lx.cover.tmp",
             (unsigned long)hash);
    if(!copy_file(source, temporary) ||
       rename(temporary, destination) < 0) {
        remove(temporary);
        destination[0] = '\0';
        return false;
    }
    return true;
}

#endif
