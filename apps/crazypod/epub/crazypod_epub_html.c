#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <string.h>

#include "file.h"

#include "crazypod_epub_html.h"

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
    bool has_text;
    bool pending_space;
    bool failed;
    int pending_breaks;
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
    return writer_raw(writer, text, size);
}

static const char *tag_name(const char *tag)
{
    return tag[0] == '/' ? tag + 1 : tag;
}

static bool tag_matches(const char *tag, const char *name)
{
    return strcmp(tag_name(tag), name) == 0;
}

static bool tag_ignored(const char *tag)
{
    return tag_matches(tag, "head") ||
           tag_matches(tag, "script") ||
           tag_matches(tag, "style") ||
           tag_matches(tag, "svg") ||
           tag_matches(tag, "math");
}

static bool tag_break(const char *tag)
{
    static const char *const names[] = {
        "p", "div", "section", "article", "header", "footer",
        "aside", "blockquote", "pre", "figure", "figcaption",
        "ul", "ol", "dl", "dt", "dd", "li", "table", "tr",
        "br", "br/", "hr", "hr/", "h1", "h2", "h3", "h4",
        "h5", "h6"
    };
    const char *name = tag_name(tag);
    size_t i;

    for(i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if(strcmp(name, names[i]) == 0)
            return true;
    }
    return false;
}

int crazypod_epub_html_decode_entity(const char *entity, char *output)
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

bool crazypod_epub_html_append_text(const char *path, int output_fd)
{
    char input[1024];
    char tag[24];
    char entity[16];
    struct html_text_writer writer = {
        .fd = output_fd,
    };
    int tag_length = 0;
    int entity_length = 0;
    int ignore_depth = 0;
    bool in_tag = false;
    bool tag_name_done = false;
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
                        else
                            ++ignore_depth;
                    }
                    else if(ignore_depth == 0 && tag_break(tag))
                        writer_break(&writer, 1);
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
    return count == 0 && writer_flush(&writer);
}

#endif
