#include <ctype.h>
#include <stddef.h>
#include <stdint.h>

#include "crazypod_collation.h"

#include "crazypod_pinyin_initial_data.inc"

static uint32_t decode_first(const unsigned char *text)
{
    uint32_t value;

    if(text == NULL || *text == '\0')
        return 0;
    if(text[0] < 0x80)
        return text[0];
    if((text[0] & 0xe0) == 0xc0 &&
       (text[1] & 0xc0) == 0x80) {
        value = ((uint32_t)(text[0] & 0x1f) << 6) |
            (uint32_t)(text[1] & 0x3f);
        return value >= 0x80 ? value : 0;
    }
    if((text[0] & 0xf0) == 0xe0 &&
       (text[1] & 0xc0) == 0x80 &&
       (text[2] & 0xc0) == 0x80) {
        value = ((uint32_t)(text[0] & 0x0f) << 12) |
            ((uint32_t)(text[1] & 0x3f) << 6) |
            (uint32_t)(text[2] & 0x3f);
        return value >= 0x800 &&
            (value < 0xd800 || value > 0xdfff) ? value : 0;
    }
    if((text[0] & 0xf8) == 0xf0 &&
       (text[1] & 0xc0) == 0x80 &&
       (text[2] & 0xc0) == 0x80 &&
       (text[3] & 0xc0) == 0x80) {
        value = ((uint32_t)(text[0] & 0x07) << 18) |
            ((uint32_t)(text[1] & 0x3f) << 12) |
            ((uint32_t)(text[2] & 0x3f) << 6) |
            (uint32_t)(text[3] & 0x3f);
        return value >= 0x10000 && value <= 0x10ffff
            ? value : 0;
    }
    return 0;
}

static const unsigned char *skip_ascii_space(const char *text)
{
    const unsigned char *cursor =
        (const unsigned char *)(text != NULL ? text : "");

    while(*cursor != '\0' && *cursor < 0x80 &&
          isspace(*cursor))
        ++cursor;
    return cursor;
}

static char table_initial(
    uint32_t codepoint, uint32_t first, uint32_t last,
    const unsigned char *table)
{
    if(codepoint < first || codepoint > last)
        return '\0';
    return (char)table[codepoint - first];
}

char crazypod_collation_initial(const char *text)
{
    const unsigned char *cursor = skip_ascii_space(text);
    uint32_t codepoint = decode_first(cursor);
    char initial;

    if(codepoint >= 'a' && codepoint <= 'z')
        return (char)(codepoint - 'a' + 'A');
    if(codepoint >= 'A' && codepoint <= 'Z')
        return (char)codepoint;
    if(codepoint >= '0' && codepoint <= '9')
        return '#';

    initial = table_initial(
        codepoint, CRAZYPOD_LATIN_INITIAL_FIRST,
        CRAZYPOD_LATIN_INITIAL_LAST,
        crazypod_latin_initials);
    if(initial == '\0')
        initial = table_initial(
            codepoint, CRAZYPOD_CJK_EXT_A_INITIAL_FIRST,
            CRAZYPOD_CJK_EXT_A_INITIAL_LAST,
            crazypod_cjk_ext_a_initials);
    if(initial == '\0')
        initial = table_initial(
            codepoint, CRAZYPOD_CJK_INITIAL_FIRST,
            CRAZYPOD_CJK_INITIAL_LAST,
            crazypod_cjk_initials);
    if(initial == '\0')
        initial = table_initial(
            codepoint, CRAZYPOD_CJK_COMPAT_INITIAL_FIRST,
            CRAZYPOD_CJK_COMPAT_INITIAL_LAST,
            crazypod_cjk_compat_initials);
    return initial >= 'A' && initial <= 'Z' ? initial : '#';
}

static int compare_folded(const char *left, const char *right)
{
    unsigned char a;
    unsigned char b;

    if(left == NULL)
        left = "";
    if(right == NULL)
        right = "";
    while(*left != '\0' && *right != '\0') {
        a = (unsigned char)tolower((unsigned char)*left++);
        b = (unsigned char)tolower((unsigned char)*right++);
        if(a != b)
            return a < b ? -1 : 1;
    }
    if(*left == *right)
        return 0;
    return *left == '\0' ? -1 : 1;
}

int crazypod_collation_compare(const char *left, const char *right)
{
    char left_initial = crazypod_collation_initial(left);
    char right_initial = crazypod_collation_initial(right);

    if(left_initial != right_initial) {
        if(left_initial == '#')
            return -1;
        if(right_initial == '#')
            return 1;
        return left_initial < right_initial ? -1 : 1;
    }
    return compare_folded(left, right);
}

bool crazypod_collation_section_target(
    int count, int current, int direction,
    crazypod_collation_title_provider title_at,
    void *context, int *target, char *key)
{
    char current_key;
    char target_key;
    int index;

    if(count <= 1 || current < 0 || current >= count ||
       direction == 0 || title_at == NULL ||
       target == NULL || key == NULL)
        return false;
    current_key = crazypod_collation_initial(
        title_at(current, context));
    if(direction > 0) {
        index = current + 1;
        while(index < count &&
              crazypod_collation_initial(
                  title_at(index, context)) == current_key)
            ++index;
        if(index >= count)
            return false;
    }
    else {
        index = current - 1;
        while(index >= 0 &&
              crazypod_collation_initial(
                  title_at(index, context)) == current_key)
            --index;
        if(index < 0)
            return false;
        target_key = crazypod_collation_initial(
            title_at(index, context));
        while(index > 0 &&
              crazypod_collation_initial(
                  title_at(index - 1, context)) == target_key)
            --index;
    }
    target_key = crazypod_collation_initial(
        title_at(index, context));
    *target = index;
    *key = target_key;
    return true;
}
