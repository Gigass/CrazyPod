#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "dir.h"
#include "file.h"
#include "crazypod_epub.h"
#include "epub/cache/crazypod_epub_cache.h"
#include "epub/cache/crazypod_epub_cover_store.h"
#include "epub/extraction/crazypod_epub_extraction.h"
#include "epub/crazypod_epub_html.h"
#include "epub/crazypod_epub_navigation.h"
#include "epub/crazypod_epub_parser.h"

#define EPUB_XML_SIZE 65536
#define EPUB_MANIFEST_MAX 128
#define EPUB_CHAPTER_MAX CRAZYPOD_EPUB_CHAPTER_MAX
#define EPUB_EXTRACT_MAX (EPUB_MANIFEST_MAX + 4)

struct epub_manifest_item {
    char id[64];
    char href[192];
    char media_type[64];
    char properties[96];
    bool readable;
    bool image;
    bool navigation;
};

static char epub_xml[EPUB_XML_SIZE];
static struct epub_manifest_item manifest[EPUB_MANIFEST_MAX];
static int manifest_count;
static struct crazypod_epub_cache_chapter epub_chapters[EPUB_CHAPTER_MAX];
static int epub_chapter_count;
static struct crazypod_epub_navigation_entry
    navigation_entries[EPUB_CHAPTER_MAX];
static struct crazypod_epub_cache_book cache_book;
static char chapter_title_buffer[4096];
static char chapter_paths[EPUB_CHAPTER_MAX][MAX_PATH];
static char epub_title[96];
static char epub_author[96];
static char epub_cover_source[MAX_PATH];
static char epub_extract_entries[EPUB_EXTRACT_MAX][MAX_PATH];
static crazypod_epub_progress_callback epub_progress_callback;
static void *epub_progress_context;
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
    count = read(fd, epub_xml, sizeof(epub_xml) - 1);
    close(fd);
    if(count <= 0)
        return false;
    epub_xml[count] = '\0';
    return true;
}

static bool parse_container(const char *root, char *opf_path,
                            size_t opf_path_size)
{
    static char container[MAX_PATH];
    static char relative[MAX_PATH];
    static char root_directory[MAX_PATH];
    const char *rootfile;
    const char *end;

    if(!crazypod_epub_join_path(container, sizeof(container), root,
                  "META-INF/container.xml") ||
       !read_text_file(container))
        return false;
    rootfile = crazypod_epub_find_start_tag(epub_xml, "rootfile");
    if(rootfile == NULL)
        return false;
    end = strchr(rootfile, '>');
    if(end == NULL ||
       !crazypod_epub_extract_attribute(rootfile, end, "full-path",
                          relative, sizeof(relative)))
        return false;
    snprintf(root_directory, sizeof(root_directory), "%s", root);
    return crazypod_epub_resolve_href(opf_path, opf_path_size, root,
                        root_directory, relative);
}

static void parse_manifest(void)
{
    const char *cursor = epub_xml;
    manifest_count = 0;
    while(manifest_count < EPUB_MANIFEST_MAX &&
          (cursor = crazypod_epub_find_start_tag(cursor, "item")) != NULL) {
        const char *end = strchr(cursor, '>');
        struct epub_manifest_item *item = &manifest[manifest_count];
        if(end == NULL)
            break;
        memset(item, 0, sizeof(*item));
        if(crazypod_epub_extract_attribute(cursor, end, "id",
                             item->id, sizeof(item->id)) &&
           crazypod_epub_extract_attribute(cursor, end, "href",
                             item->href, sizeof(item->href))) {
            crazypod_epub_extract_attribute(cursor, end, "media-type",
                              item->media_type,
                              sizeof(item->media_type));
            crazypod_epub_extract_attribute(cursor, end, "properties",
                              item->properties,
                              sizeof(item->properties));
            item->readable =
                strstr(item->media_type, "html") != NULL ||
                strstr(item->media_type, "xhtml") != NULL;
            item->image =
                strncmp(item->media_type, "image/", 6) == 0;
            item->navigation =
                crazypod_epub_token_contains(item->properties, "nav") ||
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

static bool element_text(const char *xml, const char *local_name,
                         char *output, size_t size)
{
    const char *tag = crazypod_epub_find_start_tag(xml, local_name);
    const char *start;
    const char *end;

    if(tag == NULL || (start = strchr(tag, '>')) == NULL)
        return false;
    ++start;
    end = strchr(start, '<');
    return end != NULL &&
        crazypod_epub_html_copy_markup_text(output, size, start, end);
}

static const struct epub_manifest_item *cover_manifest_item(void)
{
    const struct epub_manifest_item *fallback = NULL;
    const char *cursor;
    char cover_id[64];
    int i;

    cover_id[0] = '\0';
    cursor = epub_xml;
    while((cursor = crazypod_epub_find_start_tag(cursor, "meta")) != NULL) {
        const char *end = strchr(cursor, '>');
        char name[64];
        if(end == NULL)
            break;
        name[0] = '\0';
        if(crazypod_epub_extract_attribute(cursor, end, "name",
                             name, sizeof(name)) &&
           crazypod_epub_ascii_equal(name, "cover"))
            crazypod_epub_extract_attribute(cursor, end, "content",
                              cover_id, sizeof(cover_id));
        cursor = end + 1;
    }

    for(i = 0; i < manifest_count; ++i) {
        const struct epub_manifest_item *item = &manifest[i];
        if(!item->image)
            continue;
        if(crazypod_epub_token_contains(item->properties, "cover-image"))
            return item;
        if(cover_id[0] != '\0' &&
           strcmp(item->id, cover_id) == 0)
            fallback = item;
        else if(fallback == NULL &&
                (crazypod_epub_html_find_ascii(item->id, "cover") != NULL ||
                 crazypod_epub_html_find_ascii(item->href, "cover") != NULL))
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
        crazypod_epub_resolve_href(epub_cover_source, sizeof(epub_cover_source),
                     root, opf_directory, cover->href);

    if(epub_cover_source[0] != '\0')
        return;

    cursor = epub_xml;
    while((cursor = crazypod_epub_find_start_tag(cursor, "reference")) != NULL) {
        const char *end = strchr(cursor, '>');
        char type[64];
        char href[192];
        if(end == NULL)
            break;
        type[0] = '\0';
        href[0] = '\0';
        if(crazypod_epub_extract_attribute(cursor, end, "type",
                             type, sizeof(type)) &&
           crazypod_epub_token_contains(type, "cover") &&
           crazypod_epub_extract_attribute(cursor, end, "href",
                             href, sizeof(href)) &&
           crazypod_epub_resolve_href(epub_cover_source,
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
    start = crazypod_epub_html_find_ascii(chapter_title_buffer, "<title");
    if(start == NULL)
        start = crazypod_epub_html_find_ascii(chapter_title_buffer, "<h1");
    if(start == NULL || (start = strchr(start, '>')) == NULL)
        return false;
    ++start;
    end = crazypod_epub_html_find_ascii(start, "</title");
    if(end == NULL)
        end = crazypod_epub_html_find_ascii(start, "</h1");
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
                decoded_size = crazypod_epub_html_decode_entity(entity, decoded);
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

static int chapter_index_for_href(const char *root,
                                  const char *navigation_directory,
                                  const char *href)
{
    char resolved[MAX_PATH];
    int i;

    if(!crazypod_epub_resolve_href(resolved, sizeof(resolved), root,
                     navigation_directory, href))
        return -1;
    for(i = 0; i < epub_chapter_count; ++i) {
        if(strcmp(resolved, chapter_paths[i]) == 0)
            return i;
    }
    return -1;
}

static void apply_navigation(const char *root,
                             const char *navigation_path,
                             bool epub3)
{
    char directory[MAX_PATH];
    int count;
    int i;

    crazypod_epub_directory_of(
        directory, sizeof(directory), navigation_path);
    count = epub3
        ? crazypod_epub_navigation_parse_epub3(
              epub_xml, navigation_entries, EPUB_CHAPTER_MAX)
        : crazypod_epub_navigation_parse_ncx(
              epub_xml, navigation_entries, EPUB_CHAPTER_MAX);
    for(i = 0; i < count; ++i) {
        int chapter = chapter_index_for_href(
            root, directory, navigation_entries[i].href);
        if(chapter >= 0)
            snprintf(epub_chapters[chapter].title,
                     sizeof(epub_chapters[chapter].title),
                     "%s", navigation_entries[i].title);
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
        if(crazypod_epub_token_contains(manifest[i].properties, "nav"))
            navigation = &manifest[i];
        else if(strcmp(manifest[i].media_type,
                       "application/x-dtbncx+xml") == 0)
            ncx = &manifest[i];
    }
    if(navigation == NULL)
        navigation = ncx;
    if(navigation == NULL ||
       !crazypod_epub_resolve_href(navigation_path, sizeof(navigation_path),
                     root, opf_directory, navigation->href) ||
       !read_text_file(navigation_path))
        return;

    apply_navigation(
        root, navigation_path,
        crazypod_epub_token_contains(navigation->properties, "nav"));
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
    crazypod_epub_directory_of(opf_directory, sizeof(opf_directory), opf_path);
    parse_package_details(root, opf_directory);
    output_fd = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(output_fd < 0)
        return false;
    cursor = epub_xml;
    while((cursor = crazypod_epub_find_start_tag(cursor, "itemref")) != NULL) {
        const char *end = strchr(cursor, '>');
        char id[64];
        char linear[16];
        const struct epub_manifest_item *item;
        char chapter[MAX_PATH];
        if(end == NULL)
            break;
        linear[0] = '\0';
        crazypod_epub_extract_attribute(cursor, end, "linear",
                          linear, sizeof(linear));
        if(crazypod_epub_extract_attribute(cursor, end, "idref", id, sizeof(id))) {
            item = manifest_item(id);
            if(!crazypod_epub_ascii_equal(linear, "no") &&
               item != NULL && item->readable &&
               crazypod_epub_resolve_href(chapter, sizeof(chapter), root,
                            opf_directory, item->href)) {
                off_t chapter_offset;
                if(wrote && !write_exact(output_fd, "\n\n", 2))
                    break;
                chapter_offset = lseek(output_fd, 0, SEEK_CUR);
                if(crazypod_epub_html_append_text(chapter, output_fd)) {
                    if(epub_chapter_count < EPUB_CHAPTER_MAX) {
                        struct crazypod_epub_cache_chapter *entry =
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
    crazypod_epub_directory_of(opf_directory, sizeof(opf_directory), opf_path);
    parse_package_details(extract_root, opf_directory);

    for(i = 0; i < manifest_count && count < EPUB_EXTRACT_MAX; ++i) {
        char resolved[MAX_PATH];

        if(!manifest[i].readable &&
           !manifest[i].navigation)
            continue;
        if(crazypod_epub_resolve_href(resolved, sizeof(resolved),
                        extract_root, opf_directory,
                        manifest[i].href) &&
           crazypod_epub_extraction_archive_name(
               epub_extract_entries[count], MAX_PATH,
               extract_root, resolved))
            ++count;
    }
    if(epub_cover_source[0] != '\0' &&
       count < EPUB_EXTRACT_MAX &&
       crazypod_epub_extraction_archive_name(
           epub_extract_entries[count], MAX_PATH,
           extract_root, epub_cover_source))
        ++count;
    if(count == 0 ||
       !crazypod_epub_extraction_entries(
           epub_path, extract_root,
           epub_extract_entries, count))
        return false;

    if(epub_cover_source[0] != '\0') {
        if(!crazypod_epub_cover_is_image(epub_cover_source)) {
            char cover_entry[MAX_PATH];
            if(crazypod_epub_cover_resolve_document(
                   extract_root, epub_cover_source,
                   sizeof(epub_cover_source)) &&
               crazypod_epub_extraction_archive_name(
                   cover_entry, sizeof(cover_entry),
                   extract_root, epub_cover_source))
                crazypod_epub_extraction_entry(
                    epub_path, extract_root, cover_entry);
        }
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
    static struct crazypod_epub_cache_info info;
    static struct crazypod_epub_cache_paths paths;
    uint32_t hash = crazypod_epub_cache_path_hash(epub_path);
    static char opf_path[MAX_PATH];
    static char opf_entry[MAX_PATH];
    static char cover_entry[MAX_PATH];
    static char opf_directory[MAX_PATH];
    bool success = false;

    crazypod_epub_cache_ensure_directory();
    crazypod_epub_cache_paths(epub_path, &paths);
    if(crazypod_epub_cache_load_info(
           epub_path, source_size, source_mtime, paths.info, &info))
        goto publish;

    crazypod_epub_extraction_remove_tree(paths.probe_root);
    mkdir(paths.probe_root);
    if(!crazypod_epub_extraction_entry(
           epub_path, paths.probe_root,
           "META-INF/container.xml") ||
       !parse_container(paths.probe_root, opf_path,
                        sizeof(opf_path)) ||
       !crazypod_epub_extraction_archive_name(
           opf_entry, sizeof(opf_entry),
           paths.probe_root, opf_path) ||
       !crazypod_epub_extraction_entry(
           epub_path, paths.probe_root, opf_entry) ||
       !read_text_file(opf_path))
        goto done;

    parse_manifest();
    crazypod_epub_directory_of(opf_directory, sizeof(opf_directory), opf_path);
    parse_package_details(paths.probe_root, opf_directory);

    memset(&info, 0, sizeof(info));
    snprintf(info.title, sizeof(info.title), "%s", epub_title);
    snprintf(info.author, sizeof(info.author), "%s", epub_author);

    if(epub_cover_source[0] != '\0' &&
       crazypod_epub_extraction_archive_name(
           cover_entry, sizeof(cover_entry),
           paths.probe_root, epub_cover_source) &&
       crazypod_epub_extraction_entry(
           epub_path, paths.probe_root, cover_entry)) {
        if(!crazypod_epub_cover_is_image(epub_cover_source)) {
            if(crazypod_epub_cover_resolve_document(
                   paths.probe_root, epub_cover_source,
                   sizeof(epub_cover_source)) &&
               crazypod_epub_extraction_archive_name(
                   cover_entry, sizeof(cover_entry),
                   paths.probe_root, epub_cover_source))
                crazypod_epub_extraction_entry(
                    epub_path, paths.probe_root, cover_entry);
        }
        crazypod_epub_cover_persist(
            hash, paths.probe_root, epub_cover_source,
            info.cover_path, sizeof(info.cover_path));
    }
    if(!crazypod_epub_cache_save_info(
           epub_path, source_size, source_mtime, paths.info, &info))
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
    crazypod_epub_extraction_remove_tree(paths.probe_root);
    return success;
}

bool crazypod_epub_prepare(const char *epub_path,
                           uint32_t source_size,
                           uint32_t source_mtime,
                           char *text_path,
                           size_t text_path_size,
                           uint32_t *text_size)
{
    static struct crazypod_epub_cache_info info;
    static struct crazypod_epub_cache_paths paths;
    uint32_t hash = crazypod_epub_cache_path_hash(epub_path);
    static char opf_path[MAX_PATH];
    static char opf_entry[MAX_PATH];
    int fd;
    off_t size;
    bool success = false;

    report_prepare_progress(5, "Checking book cache");
    crazypod_epub_cache_ensure_directory();
    crazypod_epub_cache_paths(epub_path, &paths);
    snprintf(text_path, text_path_size, "%s", paths.text);
    if(crazypod_epub_cache_load_book(
           epub_path, source_size, source_mtime, paths.text,
           paths.metadata, &cache_book)) {
        epub_chapter_count = (int)cache_book.chapter_count;
        memcpy(epub_chapters, cache_book.chapters, sizeof(epub_chapters));
        snprintf(epub_title, sizeof(epub_title), "%s", cache_book.title);
        snprintf(epub_author, sizeof(epub_author), "%s", cache_book.author);
        snprintf(epub_cover_source, sizeof(epub_cover_source),
                 "%s", cache_book.cover_path);
        if(text_size != NULL)
            *text_size = cache_book.text_size;
        report_prepare_progress(100, "Book ready");
        return true;
    }

    report_prepare_progress(12, "Clearing temporary files");
    crazypod_epub_extraction_remove_tree(paths.extract_root);
    report_prepare_progress(15, "Opening EPUB package");
    mkdir(paths.extract_root);
    if(!crazypod_epub_extraction_entry(
           epub_path, paths.extract_root,
           "META-INF/container.xml"))
        goto done;
    report_prepare_progress(28, "Reading package metadata");
    if(!parse_container(paths.extract_root, opf_path, sizeof(opf_path)) ||
       !crazypod_epub_extraction_archive_name(
           opf_entry, sizeof(opf_entry),
           paths.extract_root, opf_path) ||
       !crazypod_epub_extraction_entry(
           epub_path, paths.extract_root, opf_entry))
        goto done;
    report_prepare_progress(45, "Extracting reading order");
    if(!prepare_epub_resources(
           epub_path, paths.extract_root, opf_path))
        goto done;
    report_prepare_progress(72, "Building readable text");
    if(!build_epub_text(paths.extract_root, opf_path, paths.temporary))
        goto done;
    report_prepare_progress(88, "Saving reading cache");
    fd = open(paths.temporary, O_RDONLY);
    if(fd < 0)
        goto done;
    size = filesize(fd);
    close(fd);
    if(size <= 0 || rename(paths.temporary, text_path) < 0)
        goto done;

    memset(&cache_book, 0, sizeof(cache_book));
    cache_book.text_size = (uint32_t)size;
    cache_book.chapter_count = (uint32_t)epub_chapter_count;
    snprintf(cache_book.title, sizeof(cache_book.title),
             "%s", epub_title);
    snprintf(cache_book.author, sizeof(cache_book.author),
             "%s", epub_author);
    crazypod_epub_cover_persist(
        hash, paths.extract_root, epub_cover_source,
        cache_book.cover_path, sizeof(cache_book.cover_path));
    memcpy(cache_book.chapters, epub_chapters, sizeof(cache_book.chapters));
    success = crazypod_epub_cache_save_book(
        epub_path, source_size, source_mtime, paths.metadata, &cache_book);
    if(success && text_size != NULL)
        *text_size = cache_book.text_size;
    if(success) {
        memset(&info, 0, sizeof(info));
        snprintf(info.title, sizeof(info.title),
                 "%s", cache_book.title);
        snprintf(info.author, sizeof(info.author),
                 "%s", cache_book.author);
        snprintf(info.cover_path, sizeof(info.cover_path),
                 "%s", cache_book.cover_path);
        crazypod_epub_cache_save_info(
            epub_path, source_size, source_mtime, paths.info, &info);
    }

done:
    report_prepare_progress(
        94,
        success ? "Cleaning temporary files"
                : "Cleaning failed import");
    remove(paths.temporary);
    crazypod_epub_extraction_remove_tree(paths.extract_root);
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
    const struct crazypod_epub_cache_chapter *chapter;

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
        snprintf(title, title_size, "%s", cache_book.title);
    if(author != NULL && author_size > 0)
        snprintf(author, author_size, "%s", cache_book.author);
    if(cover_path != NULL && cover_path_size > 0)
        snprintf(cover_path, cover_path_size,
                 "%s", cache_book.cover_path);
}

void crazypod_epub_remove_cache(const char *epub_path)
{
    crazypod_epub_cache_remove(epub_path);
}

#endif
