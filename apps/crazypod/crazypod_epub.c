#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "dir.h"
#include "file.h"
#include "zip.h"

#include "crazypod_epub.h"

#ifndef EPUB_CACHE_PARENT
#define EPUB_CACHE_PARENT "/.crazypod"
#endif
#ifndef EPUB_CACHE_DIRECTORY
#define EPUB_CACHE_DIRECTORY EPUB_CACHE_PARENT "/books"
#endif
#define EPUB_CACHE_MAGIC 0x43504550u
#define EPUB_CACHE_VERSION 3u
#define EPUB_INFO_MAGIC 0x43504549u
#define EPUB_XML_SIZE 65536
#define EPUB_MANIFEST_MAX 128
#define EPUB_CHAPTER_MAX 128
#define EPUB_EXTRACT_MAX (EPUB_MANIFEST_MAX + 4)

struct epub_chapter_disk {
    uint32_t offset;
    char title[96];
};

struct epub_cache_disk {
    uint32_t magic;
    uint32_t version;
    uint32_t source_size;
    uint32_t source_mtime;
    char source_path[MAX_PATH];
    uint32_t text_size;
    uint32_t chapter_count;
    char title[96];
    char author[96];
    char cover_path[MAX_PATH];
    struct epub_chapter_disk chapters[EPUB_CHAPTER_MAX];
    uint32_t checksum;
};

struct epub_manifest_item {
    char id[64];
    char href[192];
    char media_type[64];
    char properties[96];
    bool readable;
    bool image;
    bool navigation;
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

static char epub_xml[EPUB_XML_SIZE];
static struct epub_manifest_item manifest[EPUB_MANIFEST_MAX];
static int manifest_count;
static struct epub_chapter_disk epub_chapters[EPUB_CHAPTER_MAX];
static int epub_chapter_count;
static struct epub_cache_disk cache_disk;
static char chapter_title_buffer[4096];
static char chapter_paths[EPUB_CHAPTER_MAX][MAX_PATH];
static char epub_title[96];
static char epub_author[96];
static char epub_cover_source[MAX_PATH];
static char epub_extract_entries[EPUB_EXTRACT_MAX][MAX_PATH];
static crazypod_epub_progress_callback epub_progress_callback;
static void *epub_progress_context;
/*
 * EPUB metadata probing runs synchronously from the main UI thread.  The
 * target main stack is small, so all multi-kilobyte parsing and copy
 * workspaces must live in BSS rather than in nested automatic frames.
 * These routines are intentionally single-threaded.
 */
static char epub_copy_buffer[4096];

static void report_prepare_progress(int percent, const char *stage)
{
    if(epub_progress_callback != NULL)
        epub_progress_callback(percent, stage, epub_progress_context);
}

void crazypod_epub_set_progress_callback(
    crazypod_epub_progress_callback callback, void *context)
{
    epub_progress_callback = callback;
    epub_progress_context = context;
}

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

static uint32_t path_hash(const char *path)
{
    return hash_bytes(2166136261u, path, strlen(path));
}

static bool read_text_file(const char *path)
{
    int fd = open(path, O_RDONLY);
    ssize_t count;

    if(fd < 0)
        return false;
    count = read(fd, epub_xml, sizeof(epub_xml) - 1);
    close(fd);
    if(count <= 0)
        return false;
    epub_xml[count] = '\0';
    return true;
}

static bool join_path(char *output, size_t size,
                      const char *left, const char *right)
{
    int result;
    if(left[0] == '\0')
        result = snprintf(output, size, "%s", right);
    else
        result = snprintf(output, size, "%s/%s", left, right);
    return result > 0 && (size_t)result < size;
}

static int ascii_lower(int value)
{
    return value >= 'A' && value <= 'Z'
        ? value - 'A' + 'a' : value;
}

static bool ascii_equal_ignore_case(const char *left, const char *right)
{
    while(*left != '\0' && *right != '\0') {
        if(ascii_lower((unsigned char)*left) !=
           ascii_lower((unsigned char)*right))
            return false;
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

static bool xml_name_character(char value)
{
    return (value >= 'A' && value <= 'Z') ||
           (value >= 'a' && value <= 'z') ||
           (value >= '0' && value <= '9') ||
           value == '_' || value == '-' || value == '.';
}

static const char *find_start_tag(const char *cursor, const char *local_name)
{
    size_t wanted = strlen(local_name);

    while((cursor = strchr(cursor, '<')) != NULL) {
        const char *name = cursor + 1;
        const char *local;
        const char *end;

        if(*name == '/' || *name == '!' || *name == '?') {
            ++cursor;
            continue;
        }
        end = name;
        while(xml_name_character(*end) || *end == ':')
            ++end;
        local = end;
        while(local > name && local[-1] != ':')
            --local;
        if((size_t)(end - local) == wanted) {
            size_t i;
            for(i = 0; i < wanted; ++i) {
                if(ascii_lower((unsigned char)local[i]) !=
                   ascii_lower((unsigned char)local_name[i]))
                    break;
            }
            if(i == wanted)
                return cursor;
        }
        ++cursor;
    }
    return NULL;
}

static bool token_contains(const char *tokens, const char *wanted)
{
    size_t wanted_length = strlen(wanted);

    while(*tokens != '\0') {
        const char *end;
        while(*tokens == ' ' || *tokens == '\t' ||
              *tokens == '\r' || *tokens == '\n')
            ++tokens;
        end = tokens;
        while(*end != '\0' && *end != ' ' && *end != '\t' &&
              *end != '\r' && *end != '\n')
            ++end;
        if((size_t)(end - tokens) == wanted_length &&
           strncmp(tokens, wanted, wanted_length) == 0)
            return true;
        tokens = end;
    }
    return false;
}

static int hex_value(char value)
{
    if(value >= '0' && value <= '9')
        return value - '0';
    if(value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    if(value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    return -1;
}

static bool decode_href(char *output, size_t size, const char *href)
{
    size_t used = 0;
    const char *cursor = href;
    const char *colon = strchr(href, ':');
    const char *slash = strchr(href, '/');

    if(colon != NULL && (slash == NULL || colon < slash))
        return false;
    while(*cursor != '\0' && *cursor != '#' && *cursor != '?') {
        int high;
        int low;
        char value = *cursor++;

        if(value == '%' && (high = hex_value(cursor[0])) >= 0 &&
           cursor[1] != '\0' && (low = hex_value(cursor[1])) >= 0) {
            value = (char)((high << 4) | low);
            cursor += 2;
            if(value == '\0')
                return false;
        }
        if(value == '\\')
            value = '/';
        if(used + 1 >= size)
            return false;
        output[used++] = value;
    }
    output[used] = '\0';
    return used > 0;
}

static bool normalize_path(char *output, size_t size, const char *input)
{
    const char *cursor = input;
    size_t used = 0;
    bool absolute = *cursor == '/';

    if(absolute) {
        if(size < 2)
            return false;
        output[used++] = '/';
        while(*cursor == '/')
            ++cursor;
    }
    while(*cursor != '\0') {
        const char *end = strchr(cursor, '/');
        size_t length = end != NULL
            ? (size_t)(end - cursor) : strlen(cursor);

        if(length == 0 || (length == 1 && cursor[0] == '.')) {
            /* Nothing. */
        }
        else if(length == 2 && cursor[0] == '.' && cursor[1] == '.') {
            if(used <= (absolute ? 1u : 0u))
                return false;
            if(used > 0 && output[used - 1] == '/')
                --used;
            while(used > (absolute ? 1u : 0u) &&
                  output[used - 1] != '/')
                --used;
        }
        else {
            if(used > 0 && output[used - 1] != '/') {
                if(used + 1 >= size)
                    return false;
                output[used++] = '/';
            }
            if(used + length >= size)
                return false;
            memcpy(output + used, cursor, length);
            used += length;
        }
        if(end == NULL)
            break;
        cursor = end + 1;
        while(*cursor == '/')
            ++cursor;
    }
    if(used == 0)
        return false;
    output[used] = '\0';
    return true;
}

static bool resolve_href(char *output, size_t size, const char *root,
                         const char *directory, const char *href)
{
    static char decoded[MAX_PATH];
    static char combined[MAX_PATH * 2];
    static char normalized[MAX_PATH];
    int result;
    size_t root_length = strlen(root);

    if(!decode_href(decoded, sizeof(decoded), href))
        return false;
    if(decoded[0] == '/')
        result = snprintf(combined, sizeof(combined), "%s/%s",
                          root, decoded + 1);
    else
        result = snprintf(combined, sizeof(combined), "%s/%s",
                          directory, decoded);
    if(result <= 0 || (size_t)result >= sizeof(combined) ||
       !normalize_path(normalized, sizeof(normalized), combined))
        return false;
    if(strncmp(normalized, root, root_length) != 0 ||
       (normalized[root_length] != '\0' &&
        normalized[root_length] != '/'))
        return false;
    result = snprintf(output, size, "%s", normalized);
    return result >= 0 && (size_t)result < size;
}

static bool extract_attribute(const char *start, const char *end,
                              const char *name,
                              char *output, size_t size)
{
    const char *cursor = start;
    size_t name_length = strlen(name);

    while(cursor < end) {
        const char *match = strstr(cursor, name);
        const char *value;
        char quote;
        size_t length;
        if(match == NULL || match >= end)
            return false;
        if(match > start &&
           ((match[-1] >= 'A' && match[-1] <= 'Z') ||
            (match[-1] >= 'a' && match[-1] <= 'z') ||
            match[-1] == '-' || match[-1] == '_')) {
            cursor = match + name_length;
            continue;
        }
        value = match + name_length;
        while(value < end && (*value == ' ' || *value == '\t'))
            ++value;
        if(value >= end || *value != '=') {
            cursor = value;
            continue;
        }
        ++value;
        while(value < end && (*value == ' ' || *value == '\t'))
            ++value;
        if(value >= end || (*value != '"' && *value != '\''))
            return false;
        quote = *value++;
        cursor = value;
        while(cursor < end && *cursor != quote)
            ++cursor;
        if(cursor >= end)
            return false;
        length = (size_t)(cursor - value);
        if(length >= size)
            length = size - 1;
        memcpy(output, value, length);
        output[length] = '\0';
        return true;
    }
    return false;
}

static void directory_of(char *directory, size_t size, const char *path)
{
    const char *slash = strrchr(path, '/');
    size_t length = slash != NULL ? (size_t)(slash - path) : 0;
    if(length >= size)
        length = size - 1;
    memcpy(directory, path, length);
    directory[length] = '\0';
}

static bool parse_container(const char *root, char *opf_path,
                            size_t opf_path_size)
{
    static char container[MAX_PATH];
    static char relative[MAX_PATH];
    static char root_directory[MAX_PATH];
    const char *rootfile;
    const char *end;

    if(!join_path(container, sizeof(container), root,
                  "META-INF/container.xml") ||
       !read_text_file(container))
        return false;
    rootfile = find_start_tag(epub_xml, "rootfile");
    if(rootfile == NULL)
        return false;
    end = strchr(rootfile, '>');
    if(end == NULL ||
       !extract_attribute(rootfile, end, "full-path",
                          relative, sizeof(relative)))
        return false;
    snprintf(root_directory, sizeof(root_directory), "%s", root);
    return resolve_href(opf_path, opf_path_size, root,
                        root_directory, relative);
}

static void parse_manifest(void)
{
    const char *cursor = epub_xml;
    manifest_count = 0;
    while(manifest_count < EPUB_MANIFEST_MAX &&
          (cursor = find_start_tag(cursor, "item")) != NULL) {
        const char *end = strchr(cursor, '>');
        struct epub_manifest_item *item = &manifest[manifest_count];
        if(end == NULL)
            break;
        memset(item, 0, sizeof(*item));
        if(extract_attribute(cursor, end, "id",
                             item->id, sizeof(item->id)) &&
           extract_attribute(cursor, end, "href",
                             item->href, sizeof(item->href))) {
            extract_attribute(cursor, end, "media-type",
                              item->media_type,
                              sizeof(item->media_type));
            extract_attribute(cursor, end, "properties",
                              item->properties,
                              sizeof(item->properties));
            item->readable =
                strstr(item->media_type, "html") != NULL ||
                strstr(item->media_type, "xhtml") != NULL;
            item->image =
                strncmp(item->media_type, "image/", 6) == 0;
            item->navigation =
                token_contains(item->properties, "nav") ||
                strcmp(item->media_type,
                       "application/x-dtbncx+xml") == 0;
            ++manifest_count;
        }
        cursor = end + 1;
    }
}

static const struct epub_manifest_item *manifest_item(const char *id)
{
    int i;
    for(i = 0; i < manifest_count; ++i) {
        if(strcmp(manifest[i].id, id) == 0)
            return &manifest[i];
    }
    return NULL;
}

static bool tag_break(const char *tag)
{
    return strcmp(tag, "p") == 0 ||
           strcmp(tag, "/p") == 0 ||
           strcmp(tag, "div") == 0 ||
           strcmp(tag, "/div") == 0 ||
           strcmp(tag, "br") == 0 ||
           strcmp(tag, "br/") == 0 ||
           strcmp(tag, "li") == 0 ||
           strcmp(tag, "/li") == 0 ||
           strcmp(tag, "h1") == 0 ||
           strcmp(tag, "/h1") == 0 ||
           strcmp(tag, "h2") == 0 ||
           strcmp(tag, "/h2") == 0 ||
           strcmp(tag, "h3") == 0 ||
           strcmp(tag, "/h3") == 0;
}

static int decode_entity(const char *entity, char *output)
{
    if(entity[0] == '#') {
        uint32_t value = 0;
        int base = 10;
        int i = 1;
        if(entity[i] == 'x' || entity[i] == 'X') {
            base = 16;
            ++i;
        }
        for(; entity[i] != '\0'; ++i) {
            int digit;
            if(entity[i] >= '0' && entity[i] <= '9')
                digit = entity[i] - '0';
            else if(base == 16 && entity[i] >= 'a' && entity[i] <= 'f')
                digit = entity[i] - 'a' + 10;
            else if(base == 16 && entity[i] >= 'A' && entity[i] <= 'F')
                digit = entity[i] - 'A' + 10;
            else {
                value = 0;
                break;
            }
            value = value * (uint32_t)base + (uint32_t)digit;
        }
        if(value > 0 && value <= 0x7f) {
            output[0] = (char)value;
            return 1;
        }
        if(value >= 0x80 && value <= 0x7ff) {
            output[0] = (char)(0xc0 | (value >> 6));
            output[1] = (char)(0x80 | (value & 0x3f));
            return 2;
        }
        if(value >= 0x800 && value <= 0xffff &&
           !(value >= 0xd800 && value <= 0xdfff)) {
            output[0] = (char)(0xe0 | (value >> 12));
            output[1] = (char)(0x80 | ((value >> 6) & 0x3f));
            output[2] = (char)(0x80 | (value & 0x3f));
            return 3;
        }
        if(value >= 0x10000 && value <= 0x10ffff) {
            output[0] = (char)(0xf0 | (value >> 18));
            output[1] = (char)(0x80 | ((value >> 12) & 0x3f));
            output[2] = (char)(0x80 | ((value >> 6) & 0x3f));
            output[3] = (char)(0x80 | (value & 0x3f));
            return 4;
        }
    }
    if(strcmp(entity, "amp") == 0) {
        output[0] = '&';
        return 1;
    }
    if(strcmp(entity, "lt") == 0) {
        output[0] = '<';
        return 1;
    }
    if(strcmp(entity, "gt") == 0) {
        output[0] = '>';
        return 1;
    }
    if(strcmp(entity, "quot") == 0) {
        output[0] = '"';
        return 1;
    }
    if(strcmp(entity, "apos") == 0) {
        output[0] = '\'';
        return 1;
    }
    if(strcmp(entity, "nbsp") == 0) {
        output[0] = ' ';
        return 1;
    }
    if(strcmp(entity, "ndash") == 0) {
        memcpy(output, "\xe2\x80\x93", 3);
        return 3;
    }
    if(strcmp(entity, "mdash") == 0) {
        memcpy(output, "\xe2\x80\x94", 3);
        return 3;
    }
    if(strcmp(entity, "hellip") == 0) {
        memcpy(output, "\xe2\x80\xa6", 3);
        return 3;
    }
    output[0] = '?';
    return 1;
}

static void chapter_title_from_href(char *title, size_t size,
                                    const char *href)
{
    const char *name = strrchr(href, '/');
    const char *dot;
    size_t length;
    size_t i;

    name = name != NULL ? name + 1 : href;
    dot = strrchr(name, '.');
    length = dot != NULL ? (size_t)(dot - name) : strlen(name);
    if(length >= size)
        length = size - 1;
    memcpy(title, name, length);
    title[length] = '\0';
    for(i = 0; i < length; ++i) {
        if(title[i] == '_' || title[i] == '-')
            title[i] = ' ';
    }
    if(title[0] == '\0')
        snprintf(title, size, "Chapter");
}

static bool ascii_prefix_ignore_case(const char *text, const char *prefix)
{
    while(*prefix != '\0') {
        char left = *text++;
        char right = *prefix++;
        if(left == '\0')
            return false;
        if(left >= 'A' && left <= 'Z')
            left = (char)(left - 'A' + 'a');
        if(right >= 'A' && right <= 'Z')
            right = (char)(right - 'A' + 'a');
        if(left != right)
            return false;
    }
    return true;
}

static const char *find_ascii_ignore_case(const char *text,
                                          const char *needle)
{
    for(; *text != '\0'; ++text) {
        if(ascii_prefix_ignore_case(text, needle))
            return text;
    }
    return NULL;
}

static bool copy_markup_text(char *output, size_t size,
                             const char *start, const char *end)
{
    size_t used = 0;
    bool in_tag = false;
    bool pending_space = false;

    while(start < end && used + 1 < size) {
        if(*start == '<') {
            in_tag = true;
            ++start;
            continue;
        }
        if(in_tag) {
            if(*start == '>')
                in_tag = false;
            ++start;
            continue;
        }
        if(*start == '&') {
            const char *semicolon = strchr(start, ';');
            if(semicolon != NULL && semicolon < end &&
               semicolon - start < 16) {
                char entity[16];
                char decoded[4];
                int decoded_size;
                size_t entity_size =
                    (size_t)(semicolon - start - 1);

                memcpy(entity, start + 1, entity_size);
                entity[entity_size] = '\0';
                decoded_size = decode_entity(entity, decoded);
                if(decoded_size == 1 && decoded[0] == ' ')
                    pending_space = used > 0;
                else if(used + (size_t)decoded_size < size) {
                    if(pending_space && used + 1 < size)
                        output[used++] = ' ';
                    pending_space = false;
                    memcpy(output + used, decoded,
                           (size_t)decoded_size);
                    used += (size_t)decoded_size;
                }
                start = semicolon + 1;
                continue;
            }
        }
        if(*start == ' ' || *start == '\t' ||
           *start == '\r' || *start == '\n') {
            pending_space = used > 0;
            ++start;
            continue;
        }
        if(pending_space && used + 1 < size)
            output[used++] = ' ';
        pending_space = false;
        output[used++] = *start++;
    }
    output[used] = '\0';
    return used > 0;
}

static bool element_text(const char *xml, const char *local_name,
                         char *output, size_t size)
{
    const char *tag = find_start_tag(xml, local_name);
    const char *start;
    const char *end;

    if(tag == NULL || (start = strchr(tag, '>')) == NULL)
        return false;
    ++start;
    end = strchr(start, '<');
    return end != NULL &&
        copy_markup_text(output, size, start, end);
}

static const struct epub_manifest_item *cover_manifest_item(void)
{
    const struct epub_manifest_item *fallback = NULL;
    const char *cursor;
    char cover_id[64];
    int i;

    cover_id[0] = '\0';
    cursor = epub_xml;
    while((cursor = find_start_tag(cursor, "meta")) != NULL) {
        const char *end = strchr(cursor, '>');
        char name[64];
        if(end == NULL)
            break;
        name[0] = '\0';
        if(extract_attribute(cursor, end, "name",
                             name, sizeof(name)) &&
           ascii_equal_ignore_case(name, "cover"))
            extract_attribute(cursor, end, "content",
                              cover_id, sizeof(cover_id));
        cursor = end + 1;
    }

    for(i = 0; i < manifest_count; ++i) {
        const struct epub_manifest_item *item = &manifest[i];
        if(!item->image)
            continue;
        if(token_contains(item->properties, "cover-image"))
            return item;
        if(cover_id[0] != '\0' &&
           strcmp(item->id, cover_id) == 0)
            fallback = item;
        else if(fallback == NULL &&
                (find_ascii_ignore_case(item->id, "cover") != NULL ||
                 find_ascii_ignore_case(item->href, "cover") != NULL))
            fallback = item;
    }
    return fallback;
}

static void parse_package_details(const char *root,
                                  const char *opf_directory)
{
    const struct epub_manifest_item *cover;
    const char *cursor;

    epub_title[0] = '\0';
    epub_author[0] = '\0';
    epub_cover_source[0] = '\0';
    element_text(epub_xml, "title",
                 epub_title, sizeof(epub_title));
    element_text(epub_xml, "creator",
                 epub_author, sizeof(epub_author));

    cover = cover_manifest_item();
    if(cover != NULL)
        resolve_href(epub_cover_source, sizeof(epub_cover_source),
                     root, opf_directory, cover->href);

    if(epub_cover_source[0] != '\0')
        return;

    cursor = epub_xml;
    while((cursor = find_start_tag(cursor, "reference")) != NULL) {
        const char *end = strchr(cursor, '>');
        char type[64];
        char href[192];
        if(end == NULL)
            break;
        type[0] = '\0';
        href[0] = '\0';
        if(extract_attribute(cursor, end, "type",
                             type, sizeof(type)) &&
           token_contains(type, "cover") &&
           extract_attribute(cursor, end, "href",
                             href, sizeof(href)) &&
           resolve_href(epub_cover_source,
                        sizeof(epub_cover_source),
                        root, opf_directory, href))
            return;
        cursor = end + 1;
    }
}

static bool chapter_title_from_html(char *title, size_t size,
                                    const char *path)
{
    const char *start;
    const char *end;
    size_t output = 0;
    bool in_tag = false;
    int fd;
    ssize_t count;

    fd = open(path, O_RDONLY);
    if(fd < 0)
        return false;
    count = read(fd, chapter_title_buffer,
                 sizeof(chapter_title_buffer) - 1);
    close(fd);
    if(count <= 0)
        return false;
    chapter_title_buffer[count] = '\0';
    start = find_ascii_ignore_case(chapter_title_buffer, "<title");
    if(start == NULL)
        start = find_ascii_ignore_case(chapter_title_buffer, "<h1");
    if(start == NULL || (start = strchr(start, '>')) == NULL)
        return false;
    ++start;
    end = find_ascii_ignore_case(start, "</title");
    if(end == NULL)
        end = find_ascii_ignore_case(start, "</h1");
    if(end == NULL)
        return false;
    while(start < end && output + 1 < size) {
        if(*start == '<') {
            in_tag = true;
            ++start;
            continue;
        }
        if(in_tag) {
            if(*start == '>')
                in_tag = false;
            ++start;
            continue;
        }
        if(*start == '&') {
            const char *semicolon = strchr(start, ';');
            if(semicolon != NULL && semicolon < end &&
               semicolon - start < 16) {
                char entity[16];
                char decoded[4];
                int decoded_size;
                size_t entity_size =
                    (size_t)(semicolon - start - 1);
                memcpy(entity, start + 1, entity_size);
                entity[entity_size] = '\0';
                decoded_size = decode_entity(entity, decoded);
                if(output + (size_t)decoded_size < size) {
                    memcpy(title + output, decoded,
                           (size_t)decoded_size);
                    output += (size_t)decoded_size;
                }
                start = semicolon + 1;
                continue;
            }
        }
        if(*start == '\r' || *start == '\n' || *start == '\t')
            title[output++] = ' ';
        else
            title[output++] = *start;
        ++start;
    }
    while(output > 0 && title[output - 1] == ' ')
        --output;
    title[output] = '\0';
    return output > 0;
}

static bool append_html_text(const char *path, int output_fd)
{
    char input[1024];
    char tag[24];
    char entity[16];
    int tag_length = 0;
    int entity_length = 0;
    bool in_tag = false;
    bool tag_name_done = false;
    bool in_entity = false;
    bool last_space = true;
    bool ignore = false;
    int fd = open(path, O_RDONLY);
    ssize_t count;

    if(fd < 0)
        return false;
    while((count = read(fd, input, sizeof(input))) > 0) {
        ssize_t i;
        for(i = 0; i < count; ++i) {
            char value = input[i];
            if(in_tag) {
                if(value == '>') {
                    tag[tag_length] = '\0';
                    if(strcmp(tag, "script") == 0 ||
                       strcmp(tag, "style") == 0)
                        ignore = true;
                    else if(strcmp(tag, "/script") == 0 ||
                            strcmp(tag, "/style") == 0)
                        ignore = false;
                    else if(!ignore && tag_break(tag) && !last_space) {
                        if(!write_exact(output_fd, "\n", 1)) {
                            close(fd);
                            return false;
                        }
                        last_space = true;
                    }
                    in_tag = false;
                    tag_length = 0;
                    tag_name_done = false;
                }
                else if(!tag_name_done &&
                        tag_length < (int)sizeof(tag) - 1) {
                    if(value == ' ' || value == '\t' ||
                       value == '\r' || value == '\n')
                        tag_name_done = tag_length > 0;
                    else if(value >= 'A' && value <= 'Z')
                        tag[tag_length++] =
                            (char)(value - 'A' + 'a');
                    else
                        tag[tag_length++] = value;
                }
                continue;
            }
            if(value == '<') {
                in_tag = true;
                tag_length = 0;
                tag_name_done = false;
                continue;
            }
            if(ignore)
                continue;
            if(in_entity) {
                if(value == ';') {
                    char decoded[4];
                    int decoded_size;
                    entity[entity_length] = '\0';
                    decoded_size = decode_entity(entity, decoded);
                    if(!write_exact(output_fd, decoded, decoded_size)) {
                        close(fd);
                        return false;
                    }
                    last_space = decoded[0] == ' ';
                    in_entity = false;
                    entity_length = 0;
                }
                else if(entity_length < (int)sizeof(entity) - 1)
                    entity[entity_length++] = value;
                continue;
            }
            if(value == '&') {
                in_entity = true;
                entity_length = 0;
                continue;
            }
            if(value == '\r' || value == '\n' ||
               value == '\t' || value == ' ') {
                if(!last_space) {
                    if(!write_exact(output_fd, " ", 1)) {
                        close(fd);
                        return false;
                    }
                    last_space = true;
                }
                continue;
            }
            if(!write_exact(output_fd, &value, 1)) {
                close(fd);
                return false;
            }
            last_space = false;
        }
    }
    close(fd);
    return count == 0;
}

static int chapter_index_for_href(const char *root,
                                  const char *navigation_directory,
                                  const char *href)
{
    char resolved[MAX_PATH];
    int i;

    if(!resolve_href(resolved, sizeof(resolved), root,
                     navigation_directory, href))
        return -1;
    for(i = 0; i < epub_chapter_count; ++i) {
        if(strcmp(resolved, chapter_paths[i]) == 0)
            return i;
    }
    return -1;
}

static void parse_epub3_navigation(const char *root,
                                   const char *navigation_path)
{
    char directory[MAX_PATH];
    const char *nav = epub_xml;

    directory_of(directory, sizeof(directory), navigation_path);
    while((nav = find_start_tag(nav, "nav")) != NULL) {
        const char *nav_open_end = strchr(nav, '>');
        const char *nav_end;
        char type[96];
        const char *cursor;

        if(nav_open_end == NULL)
            return;
        type[0] = '\0';
        extract_attribute(nav, nav_open_end, "epub:type",
                          type, sizeof(type));
        if(type[0] == '\0')
            extract_attribute(nav, nav_open_end, "role",
                              type, sizeof(type));
        nav_end = find_ascii_ignore_case(nav_open_end, "</nav");
        if(nav_end == NULL)
            nav_end = epub_xml + strlen(epub_xml);
        if(!token_contains(type, "toc") &&
           !token_contains(type, "doc-toc")) {
            nav = nav_open_end + 1;
            continue;
        }

        cursor = nav_open_end + 1;
        while((cursor = find_start_tag(cursor, "a")) != NULL &&
              cursor < nav_end) {
            const char *open_end = strchr(cursor, '>');
            const char *close;
            char href[192];
            char title[96];
            int chapter;

            if(open_end == NULL || open_end >= nav_end)
                break;
            close = find_ascii_ignore_case(open_end, "</a");
            if(close == NULL || close > nav_end)
                break;
            href[0] = '\0';
            title[0] = '\0';
            if(extract_attribute(cursor, open_end, "href",
                                 href, sizeof(href)) &&
               copy_markup_text(title, sizeof(title),
                                open_end + 1, close) &&
               (chapter = chapter_index_for_href(
                    root, directory, href)) >= 0)
                snprintf(epub_chapters[chapter].title,
                         sizeof(epub_chapters[chapter].title),
                         "%s", title);
            cursor = close + 1;
        }
        return;
    }
}

static void parse_ncx_navigation(const char *root,
                                 const char *navigation_path)
{
    char directory[MAX_PATH];
    const char *cursor = epub_xml;

    directory_of(directory, sizeof(directory), navigation_path);
    while((cursor = find_start_tag(cursor, "navPoint")) != NULL) {
        const char *point_end =
            find_ascii_ignore_case(cursor, "</navPoint");
        const char *content = find_start_tag(cursor, "content");
        const char *text = find_start_tag(cursor, "text");
        const char *content_end;
        const char *text_start;
        const char *text_end;
        char href[192];
        char title[96];
        int chapter;

        if(point_end == NULL)
            point_end = epub_xml + strlen(epub_xml);
        if(content == NULL || content >= point_end ||
           text == NULL || text >= point_end ||
           (content_end = strchr(content, '>')) == NULL ||
           (text_start = strchr(text, '>')) == NULL) {
            ++cursor;
            continue;
        }
        ++text_start;
        text_end = strchr(text_start, '<');
        href[0] = '\0';
        title[0] = '\0';
        if(text_end != NULL && text_end < point_end &&
           extract_attribute(content, content_end, "src",
                             href, sizeof(href)) &&
           copy_markup_text(title, sizeof(title),
                            text_start, text_end) &&
           (chapter = chapter_index_for_href(
                root, directory, href)) >= 0)
            snprintf(epub_chapters[chapter].title,
                     sizeof(epub_chapters[chapter].title),
                     "%s", title);
        ++cursor;
    }
}

static void parse_navigation(const char *root,
                             const char *opf_directory)
{
    const struct epub_manifest_item *navigation = NULL;
    const struct epub_manifest_item *ncx = NULL;
    char navigation_path[MAX_PATH];
    int i;

    for(i = 0; i < manifest_count; ++i) {
        if(token_contains(manifest[i].properties, "nav"))
            navigation = &manifest[i];
        else if(strcmp(manifest[i].media_type,
                       "application/x-dtbncx+xml") == 0)
            ncx = &manifest[i];
    }
    if(navigation == NULL)
        navigation = ncx;
    if(navigation == NULL ||
       !resolve_href(navigation_path, sizeof(navigation_path),
                     root, opf_directory, navigation->href) ||
       !read_text_file(navigation_path))
        return;

    if(token_contains(navigation->properties, "nav"))
        parse_epub3_navigation(root, navigation_path);
    else
        parse_ncx_navigation(root, navigation_path);
}

static bool build_epub_text(const char *root, const char *opf_path,
                            const char *temporary)
{
    char opf_directory[MAX_PATH];
    const char *cursor;
    int output_fd;
    bool wrote = false;

    epub_chapter_count = 0;
    memset(epub_chapters, 0, sizeof(epub_chapters));
    if(!read_text_file(opf_path))
        return false;
    parse_manifest();
    directory_of(opf_directory, sizeof(opf_directory), opf_path);
    parse_package_details(root, opf_directory);
    output_fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(output_fd < 0)
        return false;
    cursor = epub_xml;
    while((cursor = find_start_tag(cursor, "itemref")) != NULL) {
        const char *end = strchr(cursor, '>');
        char id[64];
        char linear[16];
        const struct epub_manifest_item *item;
        char chapter[MAX_PATH];
        if(end == NULL)
            break;
        linear[0] = '\0';
        extract_attribute(cursor, end, "linear",
                          linear, sizeof(linear));
        if(extract_attribute(cursor, end, "idref", id, sizeof(id))) {
            item = manifest_item(id);
            if(!ascii_equal_ignore_case(linear, "no") &&
               item != NULL && item->readable &&
               resolve_href(chapter, sizeof(chapter), root,
                            opf_directory, item->href)) {
                off_t chapter_offset;
                if(wrote && !write_exact(output_fd, "\n\n", 2))
                    break;
                chapter_offset = lseek(output_fd, 0, SEEK_CUR);
                if(append_html_text(chapter, output_fd)) {
                    if(epub_chapter_count < EPUB_CHAPTER_MAX) {
                        struct epub_chapter_disk *entry =
                            &epub_chapters[epub_chapter_count++];
                        entry->offset = chapter_offset >= 0
                            ? (uint32_t)chapter_offset : 0;
                        snprintf(
                            chapter_paths[epub_chapter_count - 1],
                            sizeof(chapter_paths[0]), "%s", chapter);
                        if(!chapter_title_from_html(
                               entry->title, sizeof(entry->title),
                               chapter))
                            chapter_title_from_href(
                                entry->title, sizeof(entry->title),
                                item->href);
                    }
                    wrote = true;
                }
            }
        }
        cursor = end + 1;
    }
    if(wrote)
        parse_navigation(root, opf_directory);
    if(fsync(output_fd) < 0)
        wrote = false;
    close(output_fd);
    (void)root;
    return wrote;
}

static bool remove_tree(const char *path)
{
    while(true) {
        DIR *directory = opendir(path);
        struct DIRENT *entry;
        char child[MAX_PATH];
        bool found = false;
        bool is_directory = false;

        if(directory == NULL)
            return remove(path) == 0;
        while((entry = readdir(directory)) != NULL) {
            struct dirinfo info;

            if(strcmp(entry->d_name, ".") == 0 ||
               strcmp(entry->d_name, "..") == 0 ||
               !join_path(child, sizeof(child), path, entry->d_name))
                continue;
            info = dir_get_info(directory, entry);
            is_directory =
                (info.attribute & ATTR_DIRECTORY) != 0;
            found = true;
            break;
        }
        closedir(directory);
        if(!found)
            return rmdir(path) == 0;

        /*
         * Never mutate a FAT directory while iterating it. Rockbox's FAT
         * cursor can otherwise return the same entry again after deletion,
         * leaving EPUB preparation stuck during cleanup.
         */
        if(is_directory) {
            if(!remove_tree(child))
                return false;
        }
        else if(remove(child) < 0)
            return false;
    }
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
    while((count = read(input, epub_copy_buffer,
                        sizeof(epub_copy_buffer))) > 0) {
        if(!write_exact(output, epub_copy_buffer, (size_t)count))
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

static bool cover_image_from_document(const char *root,
                                      char *path, size_t path_size)
{
    static char directory[MAX_PATH];
    static char href[192];
    const char *tag;
    const char *end;

    if(!read_text_file(path))
        return false;
    tag = find_start_tag(epub_xml, "img");
    if(tag == NULL)
        tag = find_start_tag(epub_xml, "image");
    if(tag == NULL || (end = strchr(tag, '>')) == NULL)
        return false;
    href[0] = '\0';
    if(!extract_attribute(tag, end, "src", href, sizeof(href)) &&
       !extract_attribute(tag, end, "href", href, sizeof(href)) &&
       !extract_attribute(tag, end, "xlink:href",
                          href, sizeof(href)))
        return false;
    directory_of(directory, sizeof(directory), path);
    return resolve_href(path, path_size, root, directory, href);
}

static bool persist_cover(uint32_t hash, const char *root,
                          char *destination, size_t destination_size)
{
    static char source[MAX_PATH];
    static char temporary[MAX_PATH];
    static char old_path[MAX_PATH];
    static char extension[8];
    const char *source_extension;
    const char *known_extensions[] = {
        ".jpg", ".jpeg", ".bmp", ".png"
    };
    size_t i;

    destination[0] = '\0';
    if(epub_cover_source[0] == '\0')
        return false;
    snprintf(source, sizeof(source), "%s", epub_cover_source);
    source_extension = path_extension(source);
    if(!ascii_equal_ignore_case(source_extension, ".jpg") &&
       !ascii_equal_ignore_case(source_extension, ".jpeg") &&
       !ascii_equal_ignore_case(source_extension, ".bmp") &&
       !ascii_equal_ignore_case(source_extension, ".png")) {
        if(!cover_image_from_document(root, source, sizeof(source)))
            return false;
        source_extension = path_extension(source);
    }
    if(strlen(source_extension) >= sizeof(extension))
        return false;
    for(i = 0; source_extension[i] != '\0'; ++i)
        extension[i] = (char)ascii_lower(
            (unsigned char)source_extension[i]);
    extension[i] = '\0';
    if(!ascii_equal_ignore_case(extension, ".jpg") &&
       !ascii_equal_ignore_case(extension, ".jpeg") &&
       !ascii_equal_ignore_case(extension, ".bmp") &&
       !ascii_equal_ignore_case(extension, ".png"))
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

struct epub_extract_filter {
    const char (*entries)[MAX_PATH];
    int count;
};

static int epub_extract_filter_callback(
    const struct zip_args *args, int pass, void *ctx)
{
    const struct epub_extract_filter *filter = ctx;
    int i;

    if(pass == ZIP_PASS_START) {
        for(i = 0; i < filter->count; ++i) {
            if(strcmp(args->name, filter->entries[i]) == 0)
                return 0;
        }
        return -1;
    }
    return 0;
}

static bool archive_name_from_path(char *output, size_t size,
                                   const char *root,
                                   const char *path)
{
    size_t root_length = strlen(root);

    if(strncmp(path, root, root_length) != 0 ||
       path[root_length] != '/')
        return false;
    {
        int result = snprintf(
            output, size, "%s", path + root_length + 1);
        return result >= 0 && (size_t)result < size;
    }
}

static bool extract_archive_entries(
    const char *epub_path, const char *extract_root,
    const char entries[][MAX_PATH], int entry_count)
{
    struct epub_extract_filter filter;
    struct zip *archive;
    int result;

    filter.entries = entries;
    filter.count = entry_count;
    archive = zip_open(epub_path, false);
    if(archive == NULL)
        return false;
    result = zip_extract(
        archive, extract_root,
        epub_extract_filter_callback, &filter);
    zip_close(archive);
    return result == 0;
}

static bool extract_archive_entry(const char *epub_path,
                                  const char *extract_root,
                                  const char *entry)
{
    char single_entry[1][MAX_PATH];
    int result = snprintf(
        single_entry[0], sizeof(single_entry[0]), "%s", entry);

    return result >= 0 &&
        (size_t)result < sizeof(single_entry[0]) &&
        extract_archive_entries(
            epub_path, extract_root, single_entry, 1);
}

static bool prepare_epub_resources(const char *epub_path,
                                   const char *extract_root,
                                   const char *opf_path)
{
    char opf_directory[MAX_PATH];
    int count = 0;
    int i;

    if(!read_text_file(opf_path))
        return false;
    parse_manifest();
    directory_of(opf_directory, sizeof(opf_directory), opf_path);
    parse_package_details(extract_root, opf_directory);

    for(i = 0; i < manifest_count && count < EPUB_EXTRACT_MAX; ++i) {
        char resolved[MAX_PATH];

        if(!manifest[i].readable &&
           !manifest[i].navigation)
            continue;
        if(resolve_href(resolved, sizeof(resolved),
                        extract_root, opf_directory,
                        manifest[i].href) &&
           archive_name_from_path(
               epub_extract_entries[count], MAX_PATH,
               extract_root, resolved))
            ++count;
    }
    if(epub_cover_source[0] != '\0' &&
       count < EPUB_EXTRACT_MAX &&
       archive_name_from_path(
           epub_extract_entries[count], MAX_PATH,
           extract_root, epub_cover_source))
        ++count;
    if(count == 0 ||
       !extract_archive_entries(
           epub_path, extract_root,
           epub_extract_entries, count))
        return false;

    if(epub_cover_source[0] != '\0') {
        const char *extension = path_extension(epub_cover_source);
        if(!ascii_equal_ignore_case(extension, ".jpg") &&
           !ascii_equal_ignore_case(extension, ".jpeg") &&
           !ascii_equal_ignore_case(extension, ".bmp") &&
           !ascii_equal_ignore_case(extension, ".png")) {
            char cover_entry[MAX_PATH];
            if(cover_image_from_document(
                   extract_root, epub_cover_source,
                   sizeof(epub_cover_source)) &&
               archive_name_from_path(
                   cover_entry, sizeof(cover_entry),
                   extract_root, epub_cover_source))
                extract_archive_entry(
                    epub_path, extract_root, cover_entry);
        }
    }
    return true;
}

static bool save_epub_info(const char *path,
                           const struct epub_info_disk *info)
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
    success = write_exact(fd, info, sizeof(*info)) &&
        fsync(fd) >= 0;
    close(fd);
    if(!success || rename(temporary, path) < 0) {
        remove(temporary);
        return false;
    }
    return true;
}

bool crazypod_epub_probe(const char *epub_path,
                         uint32_t source_size,
                         uint32_t source_mtime,
                         char *title, size_t title_size,
                         char *author, size_t author_size,
                         char *cover_path, size_t cover_path_size)
{
    static struct epub_info_disk info;
    uint32_t hash = path_hash(epub_path);
    static char info_path[MAX_PATH];
    static char extract_root[MAX_PATH];
    static char opf_path[MAX_PATH];
    static char opf_entry[MAX_PATH];
    static char cover_entry[MAX_PATH];
    static char opf_directory[MAX_PATH];
    const char *extension;
    int fd;
    bool success = false;

    mkdir(EPUB_CACHE_PARENT);
    mkdir(EPUB_CACHE_DIRECTORY);
    snprintf(info_path, sizeof(info_path),
             EPUB_CACHE_DIRECTORY "/%08lx.info",
             (unsigned long)hash);
    snprintf(extract_root, sizeof(extract_root),
             EPUB_CACHE_DIRECTORY "/%08lx.probe",
             (unsigned long)hash);

    fd = open(info_path, O_RDONLY);
    if(fd >= 0) {
        bool valid = read_exact(fd, &info, sizeof(info)) &&
            info.magic == EPUB_INFO_MAGIC &&
            info.version == EPUB_CACHE_VERSION &&
            info.source_size == source_size &&
            info.source_mtime == source_mtime &&
            strcmp(info.source_path, epub_path) == 0 &&
            info.checksum == info_checksum(&info) &&
            (info.cover_path[0] == '\0' ||
             file_exists(info.cover_path));
        close(fd);
        if(valid)
            goto publish;
    }

    remove_tree(extract_root);
    mkdir(extract_root);
    if(!extract_archive_entry(
           epub_path, extract_root,
           "META-INF/container.xml") ||
       !parse_container(extract_root, opf_path,
                        sizeof(opf_path)) ||
       !archive_name_from_path(
           opf_entry, sizeof(opf_entry),
           extract_root, opf_path) ||
       !extract_archive_entry(
           epub_path, extract_root, opf_entry) ||
       !read_text_file(opf_path))
        goto done;

    parse_manifest();
    directory_of(opf_directory, sizeof(opf_directory), opf_path);
    parse_package_details(extract_root, opf_directory);

    memset(&info, 0, sizeof(info));
    info.magic = EPUB_INFO_MAGIC;
    info.version = EPUB_CACHE_VERSION;
    info.source_size = source_size;
    info.source_mtime = source_mtime;
    snprintf(info.source_path, sizeof(info.source_path),
             "%s", epub_path);
    snprintf(info.title, sizeof(info.title), "%s", epub_title);
    snprintf(info.author, sizeof(info.author), "%s", epub_author);

    if(epub_cover_source[0] != '\0' &&
       archive_name_from_path(
           cover_entry, sizeof(cover_entry),
           extract_root, epub_cover_source) &&
       extract_archive_entry(
           epub_path, extract_root, cover_entry)) {
        extension = path_extension(epub_cover_source);
        if(!ascii_equal_ignore_case(extension, ".jpg") &&
           !ascii_equal_ignore_case(extension, ".jpeg") &&
           !ascii_equal_ignore_case(extension, ".bmp") &&
           !ascii_equal_ignore_case(extension, ".png")) {
            if(cover_image_from_document(
                   extract_root, epub_cover_source,
                   sizeof(epub_cover_source)) &&
               archive_name_from_path(
                   cover_entry, sizeof(cover_entry),
                   extract_root, epub_cover_source))
                extract_archive_entry(
                    epub_path, extract_root, cover_entry);
        }
        persist_cover(hash, extract_root,
                      info.cover_path,
                      sizeof(info.cover_path));
    }
    info.checksum = info_checksum(&info);
    if(!save_epub_info(info_path, &info))
        goto done;

publish:
    if(title != NULL && title_size > 0)
        snprintf(title, title_size, "%s", info.title);
    if(author != NULL && author_size > 0)
        snprintf(author, author_size, "%s", info.author);
    if(cover_path != NULL && cover_path_size > 0)
        snprintf(cover_path, cover_path_size,
                 "%s", info.cover_path);
    success = true;

done:
    remove_tree(extract_root);
    return success;
}

bool crazypod_epub_prepare(const char *epub_path,
                           uint32_t source_size,
                           uint32_t source_mtime,
                           char *text_path,
                           size_t text_path_size,
                           uint32_t *text_size)
{
    static struct epub_info_disk info;
    uint32_t hash = path_hash(epub_path);
    static char metadata[MAX_PATH];
    static char info_path[MAX_PATH];
    static char temporary[MAX_PATH];
    static char extract_root[MAX_PATH];
    static char opf_path[MAX_PATH];
    static char opf_entry[MAX_PATH];
    int fd;
    off_t size;
    bool success = false;

    report_prepare_progress(5, "Checking book cache");
    mkdir(EPUB_CACHE_PARENT);
    mkdir(EPUB_CACHE_DIRECTORY);
    snprintf(text_path, text_path_size,
             EPUB_CACHE_DIRECTORY "/%08lx.txt", (unsigned long)hash);
    snprintf(metadata, sizeof(metadata),
             EPUB_CACHE_DIRECTORY "/%08lx.meta", (unsigned long)hash);
    snprintf(info_path, sizeof(info_path),
             EPUB_CACHE_DIRECTORY "/%08lx.info", (unsigned long)hash);
    snprintf(temporary, sizeof(temporary),
             EPUB_CACHE_DIRECTORY "/%08lx.tmp", (unsigned long)hash);
    snprintf(extract_root, sizeof(extract_root),
             EPUB_CACHE_DIRECTORY "/%08lx.epub", (unsigned long)hash);

    fd = open(metadata, O_RDONLY);
    if(fd >= 0) {
        bool valid = read_exact(fd, &cache_disk, sizeof(cache_disk)) &&
            cache_disk.magic == EPUB_CACHE_MAGIC &&
            cache_disk.version == EPUB_CACHE_VERSION &&
            cache_disk.source_size == source_size &&
            cache_disk.source_mtime == source_mtime &&
            strcmp(cache_disk.source_path, epub_path) == 0 &&
            cache_disk.chapter_count <= EPUB_CHAPTER_MAX &&
            cache_disk.checksum == cache_checksum(&cache_disk) &&
            file_exists(text_path);
        close(fd);
        if(valid) {
            epub_chapter_count = (int)cache_disk.chapter_count;
            memcpy(epub_chapters, cache_disk.chapters,
                   sizeof(epub_chapters));
            snprintf(epub_title, sizeof(epub_title),
                     "%s", cache_disk.title);
            snprintf(epub_author, sizeof(epub_author),
                     "%s", cache_disk.author);
            snprintf(epub_cover_source, sizeof(epub_cover_source),
                     "%s", cache_disk.cover_path);
            if(text_size != NULL)
                *text_size = cache_disk.text_size;
            report_prepare_progress(100, "Book ready");
            return true;
        }
    }

    report_prepare_progress(12, "Clearing temporary files");
    remove_tree(extract_root);
    report_prepare_progress(15, "Opening EPUB package");
    mkdir(extract_root);
    if(!extract_archive_entry(
           epub_path, extract_root,
           "META-INF/container.xml"))
        goto done;
    report_prepare_progress(28, "Reading package metadata");
    if(!parse_container(extract_root, opf_path, sizeof(opf_path)) ||
       !archive_name_from_path(
           opf_entry, sizeof(opf_entry),
           extract_root, opf_path) ||
       !extract_archive_entry(
           epub_path, extract_root, opf_entry))
        goto done;
    report_prepare_progress(45, "Extracting reading order");
    if(!prepare_epub_resources(
           epub_path, extract_root, opf_path))
        goto done;
    report_prepare_progress(72, "Building readable text");
    if(!build_epub_text(extract_root, opf_path, temporary))
        goto done;
    report_prepare_progress(88, "Saving reading cache");
    fd = open(temporary, O_RDONLY);
    if(fd < 0)
        goto done;
    size = filesize(fd);
    close(fd);
    if(size <= 0 || rename(temporary, text_path) < 0)
        goto done;

    memset(&cache_disk, 0, sizeof(cache_disk));
    cache_disk.magic = EPUB_CACHE_MAGIC;
    cache_disk.version = EPUB_CACHE_VERSION;
    cache_disk.source_size = source_size;
    cache_disk.source_mtime = source_mtime;
    snprintf(cache_disk.source_path, sizeof(cache_disk.source_path),
             "%s", epub_path);
    cache_disk.text_size = (uint32_t)size;
    cache_disk.chapter_count = (uint32_t)epub_chapter_count;
    snprintf(cache_disk.title, sizeof(cache_disk.title),
             "%s", epub_title);
    snprintf(cache_disk.author, sizeof(cache_disk.author),
             "%s", epub_author);
    persist_cover(hash, extract_root,
                  cache_disk.cover_path,
                  sizeof(cache_disk.cover_path));
    memcpy(cache_disk.chapters, epub_chapters,
           sizeof(cache_disk.chapters));
    cache_disk.checksum = cache_checksum(&cache_disk);
    fd = open(metadata, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd >= 0) {
        success = write_exact(fd, &cache_disk, sizeof(cache_disk));
        if(fsync(fd) < 0)
            success = false;
        close(fd);
    }
    if(success && text_size != NULL)
        *text_size = cache_disk.text_size;
    if(success) {
        memset(&info, 0, sizeof(info));
        info.magic = EPUB_INFO_MAGIC;
        info.version = EPUB_CACHE_VERSION;
        info.source_size = source_size;
        info.source_mtime = source_mtime;
        snprintf(info.source_path, sizeof(info.source_path),
                 "%s", epub_path);
        snprintf(info.title, sizeof(info.title),
                 "%s", cache_disk.title);
        snprintf(info.author, sizeof(info.author),
                 "%s", cache_disk.author);
        snprintf(info.cover_path, sizeof(info.cover_path),
                 "%s", cache_disk.cover_path);
        info.checksum = info_checksum(&info);
        save_epub_info(info_path, &info);
    }

done:
    report_prepare_progress(
        94,
        success ? "Cleaning temporary files"
                : "Cleaning failed import");
    remove(temporary);
    remove_tree(extract_root);
    report_prepare_progress(
        100,
        success ? "Book cache ready"
                : "Could not prepare book");
    return success;
}

int crazypod_epub_chapter_count(void)
{
    return epub_chapter_count;
}

bool crazypod_epub_chapter_get(int index, char *title,
                               size_t title_size, uint32_t *offset)
{
    const struct epub_chapter_disk *chapter;

    if(index < 0 || index >= epub_chapter_count)
        return false;
    chapter = &epub_chapters[index];
    if(title != NULL && title_size > 0)
        snprintf(title, title_size, "%s", chapter->title);
    if(offset != NULL)
        *offset = chapter->offset;
    return true;
}

void crazypod_epub_book_info(char *title, size_t title_size,
                             char *author, size_t author_size,
                             char *cover_path, size_t cover_path_size)
{
    if(title != NULL && title_size > 0)
        snprintf(title, title_size, "%s", cache_disk.title);
    if(author != NULL && author_size > 0)
        snprintf(author, author_size, "%s", cache_disk.author);
    if(cover_path != NULL && cover_path_size > 0)
        snprintf(cover_path, cover_path_size,
                 "%s", cache_disk.cover_path);
}

void crazypod_epub_remove_cache(const char *epub_path)
{
    uint32_t hash;
    char path[MAX_PATH];

    if(epub_path == NULL)
        return;
    hash = path_hash(epub_path);
    snprintf(path, sizeof(path),
             EPUB_CACHE_DIRECTORY "/%08lx.txt", (unsigned long)hash);
    remove(path);
    snprintf(path, sizeof(path),
             EPUB_CACHE_DIRECTORY "/%08lx.meta", (unsigned long)hash);
    remove(path);
    snprintf(path, sizeof(path),
             EPUB_CACHE_DIRECTORY "/%08lx.info", (unsigned long)hash);
    remove(path);
    snprintf(path, sizeof(path),
             EPUB_CACHE_DIRECTORY "/%08lx.tmp", (unsigned long)hash);
    remove(path);
    snprintf(path, sizeof(path),
             EPUB_CACHE_DIRECTORY "/%08lx.cover.tmp",
             (unsigned long)hash);
    remove(path);
    snprintf(path, sizeof(path),
             EPUB_CACHE_DIRECTORY "/%08lx.cover.jpg",
             (unsigned long)hash);
    remove(path);
    snprintf(path, sizeof(path),
             EPUB_CACHE_DIRECTORY "/%08lx.cover.jpeg",
             (unsigned long)hash);
    remove(path);
    snprintf(path, sizeof(path),
             EPUB_CACHE_DIRECTORY "/%08lx.cover.bmp",
             (unsigned long)hash);
    remove(path);
    snprintf(path, sizeof(path),
             EPUB_CACHE_DIRECTORY "/%08lx.cover.png",
             (unsigned long)hash);
    remove(path);
}

#endif
