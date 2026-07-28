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
                    decoded_size =
                        crazypod_epub_html_decode_entity(entity, decoded);
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

#endif
