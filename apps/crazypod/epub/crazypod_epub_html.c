#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <string.h>

#include "file.h"

#include "crazypod_epub_html.h"
#include "crazypod_epub_parser.h"

static bool write_exact(int fd, const void *buffer, size_t size)
{
    const unsigned char *bytes = buffer;

    while(size > 0) {
        ssize_t count = write(fd, bytes, size);
        if(count <= 0)
            return false;
        bytes += count;
        size -= (size_t)count;
    }
    return true;
}

struct html_text_writer {
    int fd;
    size_t used;
    uint32_t total;
    bool has_text;
    bool rich;
    bool format_active;
    bool pending_space;
    bool failed;
    int pending_breaks;
    uint32_t format_offset;
    char buffer[1024];
};

static bool writer_flush(struct html_text_writer *writer)
{
    if(writer->failed)
        return false;
    if(writer->used > 0 &&
       !write_exact(writer->fd, writer->buffer, writer->used)) {
        writer->failed = true;
        return false;
    }
    writer->used = 0;
    return true;
}

static bool writer_raw(struct html_text_writer *writer,
                       const char *text, size_t size)
{
    while(size > 0) {
        size_t available = sizeof(writer->buffer) - writer->used;
        size_t count = size < available ? size : available;

        if(available == 0) {
            if(!writer_flush(writer))
                return false;
            continue;
        }
        memcpy(writer->buffer + writer->used, text, count);
        writer->used += count;
        writer->total += (uint32_t)count;
        text += count;
        size -= count;
    }
    return true;
}

static void writer_break(struct html_text_writer *writer, int count)
{
    if(count > writer->pending_breaks)
        writer->pending_breaks = count;
    writer->pending_space = false;
}

static void writer_space(struct html_text_writer *writer)
{
    if(writer->has_text && writer->pending_breaks == 0)
        writer->pending_space = true;
}

static bool writer_text(struct html_text_writer *writer,
                        const char *text, size_t size)
{
    int i;

    if(size == 0)
        return true;
    if(writer->has_text) {
        for(i = 0; i < writer->pending_breaks; ++i) {
            if(!writer_raw(writer, "\n", 1))
                return false;
        }
        if(writer->pending_breaks == 0 && writer->pending_space &&
           !writer_raw(writer, " ", 1))
            return false;
    }
    writer->pending_breaks = 0;
    writer->pending_space = false;
    writer->has_text = true;
    writer->format_active = false;
    return writer_raw(writer, text, size);
}

static bool tag_matches(const char *tag, const char *name)
{
    const char *cursor = tag;
    size_t length;

    if(*cursor == '/')
        ++cursor;
    while(*cursor == ' ' || *cursor == '\t' ||
          *cursor == '\r' || *cursor == '\n')
        ++cursor;
    length = 0;
    while(cursor[length] != '\0' && cursor[length] != ' ' &&
          cursor[length] != '\t' && cursor[length] != '\r' &&
          cursor[length] != '\n' && cursor[length] != '/')
        ++length;
    if(length != strlen(name))
        return false;
    while(length > 0) {
        if(crazypod_epub_ascii_lower((unsigned char)*cursor++) !=
           crazypod_epub_ascii_lower((unsigned char)*name++))
            return false;
        --length;
    }
    return true;
}

static bool tag_self_closing(const char *tag)
{
    size_t length = strlen(tag);

    while(length > 0 && (tag[length - 1] == ' ' ||
                         tag[length - 1] == '\t' ||
                         tag[length - 1] == '\r' ||
                         tag[length - 1] == '\n'))
        --length;
    return length > 0 && tag[length - 1] == '/';
}

static bool tag_ignored(const char *tag)
{
    return tag_matches(tag, "head") ||
           tag_matches(tag, "title") ||
           tag_matches(tag, "meta") ||
           tag_matches(tag, "link") ||
           tag_matches(tag, "script") ||
           tag_matches(tag, "style") ||
           tag_matches(tag, "svg") ||
           tag_matches(tag, "math") ||
           tag_matches(tag, "nav") ||
           tag_matches(tag, "noscript") ||
           tag_matches(tag, "template") ||
           tag_matches(tag, "rt") ||
           tag_matches(tag, "rp");
}

static int tag_break_count(const char *tag)
{
    if(tag_matches(tag, "p") || tag_matches(tag, "blockquote") ||
       tag_matches(tag, "pre") || tag_matches(tag, "h1") ||
       tag_matches(tag, "h2") || tag_matches(tag, "h3") ||
       tag_matches(tag, "h4") || tag_matches(tag, "h5") ||
       tag_matches(tag, "h6"))
        return 2;
    if(tag_matches(tag, "div") || tag_matches(tag, "section") ||
       tag_matches(tag, "article") || tag_matches(tag, "header") ||
       tag_matches(tag, "footer") || tag_matches(tag, "aside") ||
       tag_matches(tag, "figure") || tag_matches(tag, "figcaption") ||
       tag_matches(tag, "ul") || tag_matches(tag, "ol") ||
       tag_matches(tag, "dl") || tag_matches(tag, "dt") ||
       tag_matches(tag, "dd") || tag_matches(tag, "li") ||
       tag_matches(tag, "table") || tag_matches(tag, "tr") ||
       tag_matches(tag, "br") || tag_matches(tag, "br/") ||
       tag_matches(tag, "hr") || tag_matches(tag, "hr/"))
        return 1;
    return 0;
}

static int tag_format_style(const char *tag)
{
    char style[256];
    char class_name[256];
    int result = CRAZYPOD_EPUB_FORMAT_NORMAL;

    if(tag[0] == '/')
        return -1;
    if(tag_matches(tag, "h1") || tag_matches(tag, "h2") ||
       tag_matches(tag, "h3") || tag_matches(tag, "h4") ||
       tag_matches(tag, "h5") || tag_matches(tag, "h6"))
        result = CRAZYPOD_EPUB_FORMAT_HEADING;
    else if(tag_matches(tag, "blockquote"))
        result = CRAZYPOD_EPUB_FORMAT_QUOTE;
    else if(tag_matches(tag, "pre"))
        result = CRAZYPOD_EPUB_FORMAT_PRE;
    else if(tag_matches(tag, "li"))
        result = CRAZYPOD_EPUB_FORMAT_LIST;
    else if(!tag_matches(tag, "p") && !tag_matches(tag, "div") &&
            !tag_matches(tag, "section") &&
            !tag_matches(tag, "article") &&
            !tag_matches(tag, "header") &&
            !tag_matches(tag, "footer") &&
            !tag_matches(tag, "aside") &&
            !tag_matches(tag, "figure") &&
            !tag_matches(tag, "figcaption") &&
            !tag_matches(tag, "dt") && !tag_matches(tag, "dd"))
        return -1;

    style[0] = '\0';
    if(crazypod_epub_extract_attribute(
           tag, tag + strlen(tag), "style", style, sizeof(style)) &&
       crazypod_epub_html_find_ascii(style, "text-align") != NULL &&
       crazypod_epub_html_find_ascii(style, "center") != NULL)
        result |= CRAZYPOD_EPUB_FORMAT_CENTER;
    class_name[0] = '\0';
    if(crazypod_epub_extract_attribute(
           tag, tag + strlen(tag), "class",
           class_name, sizeof(class_name)) &&
       (crazypod_epub_html_find_ascii(class_name, "center") != NULL ||
        crazypod_epub_html_find_ascii(class_name, "title") != NULL ||
        crazypod_epub_html_find_ascii(class_name, "chapter") != NULL ||
        crazypod_epub_html_find_ascii(class_name, "epigraph") != NULL))
        result |= CRAZYPOD_EPUB_FORMAT_CENTER;
    return result;
}

static bool writer_format(struct html_text_writer *writer, int style)
{
    unsigned char format[2] = {
        (unsigned char)CRAZYPOD_EPUB_FORMAT_MARKER,
        (unsigned char)style,
    };
    int i;

    if(!writer->rich)
        return true;
    if(writer->has_text) {
        for(i = 0; i < writer->pending_breaks; ++i) {
            if(!writer_raw(writer, "\n", 1))
                return false;
        }
    }
    writer->format_offset = writer->total;
    writer->format_active = true;
    writer->pending_breaks = 0;
    writer->pending_space = false;
    return writer_raw(writer, (const char *)format, sizeof(format));
}

int crazypod_epub_html_decode_entity(const char *entity, char *output)
{
    static const struct {
        const char *name;
        const char *value;
        int size;
    } named[] = {
        { "copy", "\xc2\xa9", 2 },
        { "reg", "\xc2\xae", 2 },
        { "trade", "\xe2\x84\xa2", 3 },
        { "laquo", "\xc2\xab", 2 },
        { "raquo", "\xc2\xbb", 2 },
        { "lsquo", "\xe2\x80\x98", 3 },
        { "rsquo", "\xe2\x80\x99", 3 },
        { "ldquo", "\xe2\x80\x9c", 3 },
        { "rdquo", "\xe2\x80\x9d", 3 },
        { "bull", "\xe2\x80\xa2", 3 },
        { "middot", "\xc2\xb7", 2 },
        { "sect", "\xc2\xa7", 2 },
        { "para", "\xc2\xb6", 2 },
        { "thinsp", "\xe2\x80\x89", 3 },
        { "ensp", "\xe2\x80\x82", 3 },
        { "emsp", "\xe2\x80\x83", 3 },
        { "shy", "\xc2\xad", 2 },
    };
    size_t i;

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
    for(i = 0; i < sizeof(named) / sizeof(named[0]); ++i) {
        if(strcmp(entity, named[i].name) == 0) {
            memcpy(output, named[i].value, (size_t)named[i].size);
            return named[i].size;
        }
    }
    output[0] = '?';
    return 1;
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

const char *crazypod_epub_html_find_ascii(const char *text,
                                          const char *needle)
{
    for(; *text != '\0'; ++text) {
        if(ascii_prefix_ignore_case(text, needle))
            return text;
    }
    return NULL;
}

bool crazypod_epub_html_copy_markup_text(char *output, size_t size,
                                         const char *start,
                                         const char *end)
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
                decoded_size =
                    crazypod_epub_html_decode_entity(entity, decoded);
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

static bool append_text_with_images(
    const char *path, int output_fd,
    crazypod_epub_html_image_callback image_callback, void *context,
    bool rich)
{
    char input[1024];
    char tag[512];
    char entity[16];
    struct html_text_writer writer = {
        .fd = output_fd,
        .rich = rich,
    };
    int tag_length = 0;
    int entity_length = 0;
    int ignore_depth = 0;
    bool in_tag = false;
    bool in_entity = false;
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
                    if(tag_ignored(tag)) {
                        if(tag[0] == '/') {
                            if(ignore_depth > 0)
                                --ignore_depth;
                        }
                        else if(!tag_self_closing(tag))
                            ++ignore_depth;
                    }
                    else if(ignore_depth == 0) {
                        if(tag_matches(tag, "img") &&
                           tag[0] != '/' && image_callback != NULL) {
                            char source[256];
                            uint32_t image_offset =
                                (uint32_t)writer.total +
                                (writer.has_text
                                     ? (uint32_t)writer.pending_breaks
                                     : 0);

                            if(writer.rich && writer.format_active)
                                image_offset = writer.format_offset;

                            source[0] = '\0';
                            if(crazypod_epub_extract_attribute(
                                   tag, tag + tag_length, "src",
                                   source, sizeof(source)) &&
                               image_callback(
                                   source,
                                   image_offset,
                                   context)) {
                                writer_break(&writer, 1);
                                if(!writer_text(
                                       &writer,
                                       (char[]){ CRAZYPOD_EPUB_IMAGE_MARKER },
                                       1)) {
                                    close(fd);
                                    return false;
                                }
                                writer_break(&writer, 1);
                            }
                        }
                        else {
                            int style = tag_format_style(tag);

                            if(style >= 0 && writer.rich) {
                                if(!writer_format(&writer, style)) {
                                    close(fd);
                                    return false;
                                }
                            }
                            else {
                                int breaks = tag_break_count(tag);

                                if(breaks > 0)
                                    writer_break(&writer, breaks);
                            }
                        }
                    }
                    in_tag = false;
                    tag_length = 0;
                }
                else if(tag_length < (int)sizeof(tag) - 1)
                    tag[tag_length++] = value;
                continue;
            }
            if(value == '<') {
                in_tag = true;
                tag_length = 0;
                continue;
            }
            if(ignore_depth > 0)
                continue;
            if(in_entity) {
                if(value == ';') {
                    char decoded[4];
                    int decoded_size;
                    entity[entity_length] = '\0';
                    decoded_size =
                        crazypod_epub_html_decode_entity(entity, decoded);
                    if(decoded_size == 1 && decoded[0] == ' ')
                        writer_space(&writer);
                    else if(!writer_text(
                                &writer, decoded,
                                (size_t)decoded_size)) {
                        close(fd);
                        return false;
                    }
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
                writer_space(&writer);
                continue;
            }
            if(!writer_text(&writer, &value, 1)) {
                close(fd);
                return false;
            }
        }
    }
    close(fd);
    if(in_entity) {
        if(!writer_text(&writer, "&", 1) ||
           !writer_text(&writer, entity, (size_t)entity_length))
            return false;
    }
    return count == 0 && writer_flush(&writer);
}

bool crazypod_epub_html_append_text_with_images(
    const char *path, int output_fd,
    crazypod_epub_html_image_callback image_callback, void *context)
{
    return append_text_with_images(
        path, output_fd, image_callback, context, false);
}

bool crazypod_epub_html_append_rich_text_with_images(
    const char *path, int output_fd,
    crazypod_epub_html_image_callback image_callback, void *context)
{
    return append_text_with_images(
        path, output_fd, image_callback, context, true);
}

bool crazypod_epub_html_append_text(const char *path, int output_fd)
{
    return crazypod_epub_html_append_text_with_images(
        path, output_fd, NULL, NULL);
}

#endif
