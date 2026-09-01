#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <string.h>

#include "crazypod_epub_html.h"
#include "crazypod_epub_layout.h"

static bool is_continuation(unsigned char value)
{
    return (value & 0xc0) == 0x80;
}

static size_t utf8_decode(const unsigned char *data, size_t count,
                          uint32_t *codepoint)
{
    unsigned char first;
    uint32_t value;
    size_t bytes;
    size_t i;

    if(data == NULL || count == 0 || codepoint == NULL)
        return 0;
    first = data[0];
    if(first < 0x80) {
        *codepoint = first;
        return 1;
    }
    if(first >= 0xc2 && first <= 0xdf) {
        bytes = 2;
        value = first & 0x1f;
    }
    else if(first >= 0xe0 && first <= 0xef) {
        bytes = 3;
        value = first & 0x0f;
    }
    else if(first >= 0xf0 && first <= 0xf4) {
        bytes = 4;
        value = first & 0x07;
    }
    else {
        *codepoint = 0xfffd;
        return 1;
    }
    if(bytes > count)
        return 0;
    for(i = 1; i < bytes; ++i) {
        if(!is_continuation(data[i])) {
            *codepoint = 0xfffd;
            return 1;
        }
        value = (value << 6) | (data[i] & 0x3f);
    }
    if((bytes == 3 && value < 0x800) ||
       (bytes == 4 && value < 0x10000) ||
       (value >= 0xd800 && value <= 0xdfff) || value > 0x10ffff) {
        *codepoint = 0xfffd;
        return 1;
    }
    *codepoint = value;
    return bytes;
}

static bool is_combining(uint32_t codepoint)
{
    return (codepoint >= 0x0300 && codepoint <= 0x036f) ||
           (codepoint >= 0x1ab0 && codepoint <= 0x1aff) ||
           (codepoint >= 0x1dc0 && codepoint <= 0x1dff) ||
           (codepoint >= 0x20d0 && codepoint <= 0x20ff) ||
           (codepoint >= 0xfe20 && codepoint <= 0xfe2f) ||
           codepoint == 0xfe0e || codepoint == 0xfe0f;
}

static bool is_wide(uint32_t codepoint)
{
    return (codepoint >= 0x1100 && codepoint <= 0x115f) ||
           codepoint == 0x2329 || codepoint == 0x232a ||
           (codepoint >= 0x2e80 && codepoint <= 0x303e) ||
           (codepoint >= 0x3040 && codepoint <= 0xa4cf) ||
           (codepoint >= 0xac00 && codepoint <= 0xd7a3) ||
           (codepoint >= 0xf900 && codepoint <= 0xfaff) ||
           (codepoint >= 0xfe10 && codepoint <= 0xfe6f) ||
           (codepoint >= 0xff01 && codepoint <= 0xff60) ||
           (codepoint >= 0xffe0 && codepoint <= 0xffe6) ||
           (codepoint >= 0x1f300 && codepoint <= 0x1faff) ||
           (codepoint >= 0x20000 && codepoint <= 0x3fffd);
}

static bool is_space(uint32_t codepoint)
{
    return codepoint == ' ' || codepoint == '\t' ||
           codepoint == '\f' || codepoint == 0x00a0 ||
           codepoint == 0x2009 || codepoint == 0x200a;
}

static bool is_no_line_start(uint32_t codepoint)
{
    return codepoint == 0x3001 || codepoint == 0x3002 ||
           codepoint == 0xff0c || codepoint == 0xff0e ||
           codepoint == 0xff01 || codepoint == 0xff1f ||
           codepoint == 0x3009 || codepoint == 0x300b ||
           codepoint == 0x300d || codepoint == 0x300f ||
           codepoint == 0x3011 || codepoint == 0x2019 ||
           codepoint == 0x201d || codepoint == 0x2026 ||
           codepoint == ')' || codepoint == ']' || codepoint == '}' ||
           codepoint == ',' || codepoint == '.' || codepoint == '!' ||
           codepoint == '?' || codepoint == ':' || codepoint == ';';
}

static bool is_no_line_end(uint32_t codepoint)
{
    return codepoint == 0x3008 || codepoint == 0x300a ||
           codepoint == 0x300c || codepoint == 0x300e ||
           codepoint == 0x3010 || codepoint == 0x2018 ||
           codepoint == 0x201c || codepoint == '(' ||
           codepoint == '[' || codepoint == '{';
}

static unsigned codepoint_units(uint32_t codepoint)
{
    if(is_combining(codepoint) || codepoint == 0x200b)
        return 0;
    return is_wide(codepoint) ? 2 : 1;
}

static size_t source_bytes_at(const unsigned char *source, size_t count,
                              size_t offset, bool source_is_gbk)
{
    if(source == NULL || offset >= count)
        return 0;
    if(!source_is_gbk)
        return 1;
    if(source[offset] < 0x80)
        return 1;
    return offset + 1 < count ? 2 : 0;
}

static bool append_bytes(char *output, size_t output_size, size_t *used,
                         const unsigned char *bytes, size_t count)
{
    if(*used + count + 1 > output_size)
        return false;
    memcpy(output + *used, bytes, count);
    *used += count;
    return true;
}

size_t crazypod_epub_layout_page(
    const unsigned char *utf8, size_t utf8_count,
    const unsigned char *source, size_t source_count,
    bool source_is_gbk, bool markdown,
    char *output, size_t output_size,
    unsigned max_lines, unsigned max_line_units)
{
    size_t input = 0;
    size_t source_input = 0;
    size_t used = 0;
    size_t line_start = 0;
    size_t last_break_input = SIZE_MAX;
    size_t last_break_source = SIZE_MAX;
    size_t last_break_output = SIZE_MAX;
    unsigned lines = 1;
    unsigned line_units = 0;
    uint32_t previous = 0;
    bool have_previous = false;
    bool pending_space = false;

    if(output == NULL || output_size == 0)
        return 0;
    output[0] = '\0';
    if(utf8 == NULL || source == NULL || max_lines == 0 ||
       max_line_units == 0)
        return 0;

    while(input < utf8_count && source_input < source_count) {
        uint32_t codepoint;
        size_t character_bytes = utf8_decode(
            utf8 + input, utf8_count - input, &codepoint);
        size_t source_bytes;
        unsigned units;
        bool can_break;

        if(character_bytes == 0)
            break;
        source_bytes = source_is_gbk
            ? source_bytes_at(source, source_count, source_input, true)
            : character_bytes;
        if(source_bytes == 0 || source_input + source_bytes > source_count)
            break;
        if(markdown && (codepoint == '#' || codepoint == '*' ||
                        codepoint == '`')) {
            input += character_bytes;
            source_input += source_bytes;
            continue;
        }
        if(codepoint == '\r') {
            input += character_bytes;
            source_input += source_bytes;
            continue;
        }
        if(codepoint == CRAZYPOD_EPUB_IMAGE_MARKER) {
            if(used != 0)
                break;
            input += character_bytes;
            source_input += source_bytes;
            return source_input;
        }
        if(codepoint == 0xfeff || codepoint == 0x00ad) {
            input += character_bytes;
            source_input += source_bytes;
            if(codepoint == 0x00ad && line_units != 0) {
                last_break_input = input;
                last_break_source = source_input;
                last_break_output = used;
            }
            continue;
        }
        if(codepoint == '\n' || codepoint == 0x2028 ||
           codepoint == 0x2029) {
            if(used == 0 || line_units == 0) {
                input += character_bytes;
                source_input += source_bytes;
                pending_space = false;
                have_previous = false;
                continue;
            }
            if(lines >= max_lines)
                break;
            if(!append_bytes(output, output_size, &used,
                             (const unsigned char *)"\n", 1))
                break;
            input += character_bytes;
            source_input += source_bytes;
            ++lines;
            line_start = used;
            line_units = 0;
            last_break_input = SIZE_MAX;
            last_break_source = SIZE_MAX;
            last_break_output = SIZE_MAX;
            pending_space = false;
            have_previous = false;
            continue;
        }
        if(is_space(codepoint)) {
            input += character_bytes;
            source_input += source_bytes;
            if(line_units != 0) {
                pending_space = true;
                last_break_input = input;
                last_break_source = source_input;
                last_break_output = used;
            }
            continue;
        }

        can_break = line_units != 0 && !is_no_line_start(codepoint) &&
            ((pending_space && !is_no_line_end(previous)) ||
             is_wide(codepoint) || (have_previous && is_wide(previous)));
        if(can_break) {
            last_break_input = input;
            last_break_source = source_input;
            last_break_output = used;
        }
        if(pending_space && !(is_wide(codepoint) &&
                              have_previous && is_wide(previous)))
            units = 1 + codepoint_units(codepoint);
        else
            units = codepoint_units(codepoint);
        if(units == 0)
            units = 1;

        if(line_units != 0 && line_units + units > max_line_units) {
            if(last_break_input != SIZE_MAX &&
               last_break_output > line_start &&
               last_break_input <= input) {
                input = last_break_input;
                source_input = last_break_source;
                used = last_break_output;
            }
            if(lines >= max_lines)
                break;
            if(!append_bytes(output, output_size, &used,
                             (const unsigned char *)"\n", 1))
                break;
            ++lines;
            line_start = used;
            line_units = 0;
            last_break_input = SIZE_MAX;
            last_break_source = SIZE_MAX;
            last_break_output = SIZE_MAX;
            pending_space = false;
            have_previous = false;
            continue;
        }
        if(used + character_bytes + 1 > output_size ||
           (pending_space && !(is_wide(codepoint) &&
                               have_previous && is_wide(previous)) &&
            used + character_bytes + 2 > output_size))
            break;
        if(pending_space && !(is_wide(codepoint) &&
                              have_previous && is_wide(previous))) {
            if(!append_bytes(output, output_size, &used,
                             (const unsigned char *)" ", 1))
                break;
            ++line_units;
        }
        if(!append_bytes(output, output_size, &used,
                         utf8 + input, character_bytes))
            break;
        input += character_bytes;
        source_input += source_bytes;
        line_units += codepoint_units(codepoint);
        pending_space = false;
        previous = codepoint;
        have_previous = true;
    }
    output[used] = '\0';
    return source_input;
}

#endif
