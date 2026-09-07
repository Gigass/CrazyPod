#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "file.h"

#include "crazypod_epub_parser.h"

bool crazypod_epub_join_path(
    char *output, size_t size, const char *left, const char *right)
{
    int result;

    if(left[0] == '\0')
        result = snprintf(output, size, "%s", right);
    else
        result = snprintf(output, size, "%s/%s", left, right);
    return result > 0 && (size_t)result < size;
}

int crazypod_epub_ascii_lower(int value)
{
    return value >= 'A' && value <= 'Z'
        ? value - 'A' + 'a' : value;
}

bool crazypod_epub_ascii_equal(const char *left, const char *right)
{
    while(*left != '\0' && *right != '\0') {
        if(crazypod_epub_ascii_lower((unsigned char)*left) !=
           crazypod_epub_ascii_lower((unsigned char)*right))
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

const char *crazypod_epub_find_start_tag(
    const char *cursor, const char *local_name)
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
                if(crazypod_epub_ascii_lower(
                       (unsigned char)local[i]) !=
                   crazypod_epub_ascii_lower(
                       (unsigned char)local_name[i]))
                    break;
            }
            if(i == wanted)
                return cursor;
        }
        ++cursor;
    }
    return NULL;
}

bool crazypod_epub_token_contains(
    const char *tokens, const char *wanted)
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
           cursor[1] != '\0' &&
           (low = hex_value(cursor[1])) >= 0) {
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

static bool normalize_path(
    char *output, size_t size, const char *input)
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

bool crazypod_epub_resolve_href(
    char *output, size_t size, const char *root,
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
        result = snprintf(
            combined, sizeof(combined), "%s/%s", root, decoded + 1);
    else
        result = snprintf(
            combined, sizeof(combined), "%s/%s", directory, decoded);
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

bool crazypod_epub_extract_attribute(
    const char *start, const char *end, const char *name,
    char *output, size_t size)
{
    const char *cursor = start;
    size_t name_length = strlen(name);

    while(cursor < end) {
        const char *match = cursor;
        const char *value;
        char quote;
        size_t length;

        while(match < end &&
              crazypod_epub_ascii_lower((unsigned char)*match) !=
              crazypod_epub_ascii_lower((unsigned char)name[0]))
            ++match;
        if(match == NULL || match >= end)
            return false;
        if(match + name_length > end) {
            cursor = match + 1;
            continue;
        }
        {
            size_t i;

            for(i = 0; i < name_length; ++i) {
                if(crazypod_epub_ascii_lower(
                       (unsigned char)match[i]) !=
                   crazypod_epub_ascii_lower(
                       (unsigned char)name[i]))
                    break;
            }
            if(i != name_length) {
                cursor = match + 1;
                continue;
            }
        }
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

void crazypod_epub_directory_of(
    char *directory, size_t size, const char *path)
{
    const char *slash = strrchr(path, '/');
    size_t length = slash != NULL ? (size_t)(slash - path) : 0;

    if(length >= size)
        length = size - 1;
    memcpy(directory, path, length);
    directory[length] = '\0';
}

#endif
