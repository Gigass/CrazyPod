#include "config.h"

#ifdef IPOD_6G

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "file.h"

#include "crazypod_lyrics.h"

#define CRAZYPOD_LYRIC_LINE_COUNT 192
#define CRAZYPOD_LYRIC_TEXT_POOL_SIZE (12 * 1024)
#define CRAZYPOD_LRC_LINE_SIZE 768
#define CRAZYPOD_LRC_TIMESTAMP_COUNT 8

struct crazypod_lyric_line {
    uint32_t time_ms;
    uint16_t text_offset;
    uint16_t text_length;
};

static struct crazypod_lyric_line
    lyric_lines[CRAZYPOD_LYRIC_LINE_COUNT];
static char lyric_text_pool[CRAZYPOD_LYRIC_TEXT_POOL_SIZE];
static int lyric_count;
static size_t lyric_text_used;
static size_t plain_text_length;
static int32_t lyric_offset_ms;
static enum crazypod_lyrics_status lyric_status;
static char loaded_track_path[MAX_PATH];

static void clear_document(void)
{
    lyric_count = 0;
    lyric_text_used = 0;
    plain_text_length = 0;
    lyric_offset_ms = 0;
    lyric_text_pool[0] = '\0';
    lyric_status = CRAZYPOD_LYRICS_EMPTY;
}

static int read_lrc_line(
    int fd, char *buffer, int size, bool *overflow)
{
    int used = 0;
    char value;
    bool saw_data = false;

    if(size <= 1)
        return -1;
    *overflow = false;
    while(read(fd, &value, 1) == 1) {
        saw_data = true;
        if(value == '\n')
            break;
        if(value == '\r')
            continue;
        if(used + 1 < size)
            buffer[used++] = value;
        else
            *overflow = true;
    }
    buffer[used] = '\0';
    return saw_data ? used : -1;
}

static bool utf8_valid(const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;

    while(*cursor != 0) {
        unsigned value = *cursor;
        int continuation;

        if(value < 0x80) {
            ++cursor;
            continue;
        }
        if(value >= 0xc2 && value <= 0xdf) {
            continuation = 1;
        }
        else if(value >= 0xe0 && value <= 0xef) {
            if((value == 0xe0 && cursor[1] < 0xa0) ||
               (value == 0xed && cursor[1] >= 0xa0))
                return false;
            continuation = 2;
        }
        else if(value >= 0xf0 && value <= 0xf4) {
            if((value == 0xf0 && cursor[1] < 0x90) ||
               (value == 0xf4 && cursor[1] >= 0x90))
                return false;
            continuation = 3;
        }
        else
            return false;
        ++cursor;
        while(continuation-- > 0) {
            if((*cursor & 0xc0) != 0x80)
                return false;
            ++cursor;
        }
    }
    return true;
}

static bool parse_unsigned(
    const char **cursor, const char *end, unsigned *value)
{
    const char *text = *cursor;
    unsigned result = 0;
    bool found = false;

    while(text < end && *text >= '0' && *text <= '9') {
        if(result > (UINT_MAX - 9u) / 10u)
            return false;
        result = result * 10u + (unsigned)(*text - '0');
        found = true;
        ++text;
    }
    if(!found)
        return false;
    *cursor = text;
    *value = result;
    return true;
}

static bool parse_timestamp(
    const char *open, const char *close, uint32_t *time_ms)
{
    const char *cursor = open + 1;
    unsigned minutes;
    unsigned seconds;
    unsigned fraction = 0;
    unsigned fraction_digits = 0;
    uint64_t total;

    if(!parse_unsigned(&cursor, close, &minutes) ||
       cursor >= close || *cursor++ != ':' ||
       !parse_unsigned(&cursor, close, &seconds))
        return false;
    if(cursor < close && (*cursor == '.' || *cursor == ':')) {
        ++cursor;
        while(cursor < close && *cursor >= '0' && *cursor <= '9' &&
              fraction_digits < 3) {
            fraction = fraction * 10u +
                (unsigned)(*cursor - '0');
            ++fraction_digits;
            ++cursor;
        }
    }
    if(cursor != close || seconds >= 60)
        return false;
    if(fraction_digits == 1)
        fraction *= 100u;
    else if(fraction_digits == 2)
        fraction *= 10u;
    total = ((uint64_t)minutes * 60u + seconds) * 1000u + fraction;
    if(total > UINT32_MAX)
        return false;
    *time_ms = (uint32_t)total;
    return true;
}

static bool parse_offset_tag(const char *line, int32_t *offset_ms)
{
    const char *cursor;
    const char *close;
    int sign = 1;
    unsigned value;

    if(strncmp(line, "[offset:", 8) != 0)
        return false;
    cursor = line + 8;
    close = strchr(cursor, ']');
    if(close == NULL)
        return false;
    if(*cursor == '+' || *cursor == '-') {
        if(*cursor++ == '-')
            sign = -1;
    }
    if(!parse_unsigned(&cursor, close, &value) || cursor != close ||
       value > INT32_MAX)
        return false;
    *offset_ms = sign * (int32_t)value;
    return true;
}

static bool metadata_tag(const char *line)
{
    static const char *const tags[] = {
        "[ar:", "[al:", "[ti:", "[au:", "[by:",
        "[re:", "[ve:", "[length:", "[language:"
    };
    size_t index;

    for(index = 0; index < sizeof(tags) / sizeof(tags[0]); ++index) {
        if(strncmp(line, tags[index], strlen(tags[index])) == 0)
            return true;
    }
    return false;
}

static bool reserve_text(
    const char *text, uint16_t *offset, uint16_t *length)
{
    size_t bytes = strlen(text);

    if(bytes > UINT16_MAX ||
       lyric_text_used + bytes + 1 > sizeof(lyric_text_pool))
        return false;
    *offset = (uint16_t)lyric_text_used;
    *length = (uint16_t)bytes;
    memcpy(lyric_text_pool + lyric_text_used, text, bytes + 1);
    lyric_text_used += bytes + 1;
    return true;
}

static bool add_lyric_line(uint32_t time_ms, const char *text)
{
    struct crazypod_lyric_line line;
    int insert_at;

    if(lyric_count >= CRAZYPOD_LYRIC_LINE_COUNT ||
       !reserve_text(text, &line.text_offset, &line.text_length))
        return false;
    line.time_ms = time_ms;
    insert_at = lyric_count;
    while(insert_at > 0 &&
          lyric_lines[insert_at - 1].time_ms > time_ms) {
        lyric_lines[insert_at] = lyric_lines[insert_at - 1];
        --insert_at;
    }
    lyric_lines[insert_at] = line;
    ++lyric_count;
    return true;
}

static bool append_plain_line(const char *text)
{
    size_t bytes = strlen(text);
    bool separator = plain_text_length > 0;
    size_t required = bytes + (separator ? 1u : 0u) + 1u;

    if(plain_text_length + required > sizeof(lyric_text_pool))
        return false;
    if(separator)
        lyric_text_pool[plain_text_length++] = '\n';
    memcpy(lyric_text_pool + plain_text_length, text, bytes);
    plain_text_length += bytes;
    lyric_text_pool[plain_text_length] = '\0';
    lyric_text_used = plain_text_length + 1;
    return true;
}

static bool parse_lrc_line(char *line, bool *saw_timestamp)
{
    uint32_t timestamps[CRAZYPOD_LRC_TIMESTAMP_COUNT];
    int timestamp_count = 0;
    char *cursor = line;
    int32_t offset;
    int index;

    if(strlen(cursor) >= 3 &&
       (unsigned char)cursor[0] == 0xef &&
       (unsigned char)cursor[1] == 0xbb &&
       (unsigned char)cursor[2] == 0xbf)
        cursor += 3;
    if(!utf8_valid(cursor)) {
        lyric_status = CRAZYPOD_LYRICS_INVALID;
        return false;
    }
    if(parse_offset_tag(cursor, &offset)) {
        lyric_offset_ms = offset;
        return true;
    }
    while(*cursor == '[' &&
          timestamp_count < CRAZYPOD_LRC_TIMESTAMP_COUNT) {
        char *close = strchr(cursor, ']');
        uint32_t time_ms;

        if(close == NULL ||
           !parse_timestamp(cursor, close, &time_ms))
            break;
        timestamps[timestamp_count++] = time_ms;
        cursor = close + 1;
    }
    if(timestamp_count > 0) {
        if(!*saw_timestamp) {
            lyric_count = 0;
            lyric_text_used = 0;
            plain_text_length = 0;
            lyric_text_pool[0] = '\0';
            *saw_timestamp = true;
        }
        while(*cursor == ' ' || *cursor == '\t')
            ++cursor;
        for(index = 0; index < timestamp_count; ++index) {
            if(!add_lyric_line(timestamps[index], cursor)) {
                lyric_status = CRAZYPOD_LYRICS_CAPACITY;
                return false;
            }
        }
        return true;
    }
    if(*saw_timestamp || cursor[0] == '\0' || metadata_tag(cursor))
        return true;
    if(!append_plain_line(cursor)) {
        lyric_status = CRAZYPOD_LYRICS_CAPACITY;
        return false;
    }
    return true;
}

static void apply_offset(void)
{
    int index;

    if(lyric_offset_ms == 0)
        return;
    for(index = 0; index < lyric_count; ++index) {
        int64_t adjusted =
            (int64_t)lyric_lines[index].time_ms + lyric_offset_ms;

        if(adjusted < 0)
            adjusted = 0;
        if(adjusted > UINT32_MAX)
            adjusted = UINT32_MAX;
        lyric_lines[index].time_ms = (uint32_t)adjusted;
    }
}

static bool build_lrc_path(
    const char *track_path, char *path, size_t size, bool uppercase)
{
    char *slash;
    char *dot;

    if(track_path == NULL || track_path[0] != '/')
        return false;
    snprintf(path, size, "%s", track_path);
    slash = strrchr(path, '/');
    dot = strrchr(path, '.');
    if(dot == NULL || (slash != NULL && dot < slash))
        dot = path + strlen(path);
    if((size_t)(dot - path) + 5 > size)
        return false;
    snprintf(dot, size - (size_t)(dot - path),
             uppercase ? ".LRC" : ".lrc");
    return true;
}

bool crazypod_lyrics_load(const char *track_path)
{
    char path[MAX_PATH];
    char line[CRAZYPOD_LRC_LINE_SIZE];
    bool saw_timestamp = false;
    bool overflow;
    int fd;

    if(track_path != NULL &&
       strcmp(loaded_track_path, track_path) == 0 &&
       (lyric_status == CRAZYPOD_LYRICS_SYNCED ||
        lyric_status == CRAZYPOD_LYRICS_PLAIN))
        return true;
    clear_document();
    loaded_track_path[0] = '\0';
    if(track_path != NULL)
        snprintf(loaded_track_path, sizeof(loaded_track_path),
                 "%s", track_path);
    if(!build_lrc_path(track_path, path, sizeof(path), false)) {
        lyric_status = CRAZYPOD_LYRICS_INVALID;
        return false;
    }
    fd = open(path, O_RDONLY);
    if(fd < 0 && build_lrc_path(
           track_path, path, sizeof(path), true))
        fd = open(path, O_RDONLY);
    if(fd < 0) {
        lyric_status = CRAZYPOD_LYRICS_NOT_FOUND;
        return false;
    }
    while(read_lrc_line(fd, line, sizeof(line), &overflow) >= 0) {
        if(overflow) {
            lyric_status = CRAZYPOD_LYRICS_CAPACITY;
            close(fd);
            lyric_count = 0;
            lyric_text_pool[0] = '\0';
            return false;
        }
        if(!parse_lrc_line(line, &saw_timestamp)) {
            close(fd);
            lyric_count = 0;
            lyric_text_pool[0] = '\0';
            return false;
        }
    }
    close(fd);
    if(saw_timestamp && lyric_count > 0) {
        apply_offset();
        lyric_status = CRAZYPOD_LYRICS_SYNCED;
        return true;
    }
    if(plain_text_length > 0) {
        lyric_status = CRAZYPOD_LYRICS_PLAIN;
        return true;
    }
    lyric_status = CRAZYPOD_LYRICS_EMPTY;
    return false;
}

bool crazypod_lyrics_available(void)
{
    return lyric_status == CRAZYPOD_LYRICS_SYNCED ||
        lyric_status == CRAZYPOD_LYRICS_PLAIN;
}

bool crazypod_lyrics_synchronized(void)
{
    return lyric_status == CRAZYPOD_LYRICS_SYNCED;
}

enum crazypod_lyrics_status crazypod_lyrics_get_status(void)
{
    return lyric_status;
}

int crazypod_lyrics_line_count(void)
{
    return lyric_status == CRAZYPOD_LYRICS_SYNCED ? lyric_count : 0;
}

const char *crazypod_lyrics_line_text(int index)
{
    if(index < 0 || index >= lyric_count)
        return "";
    return lyric_text_pool + lyric_lines[index].text_offset;
}

uint32_t crazypod_lyrics_line_time(int index)
{
    return index >= 0 && index < lyric_count
        ? lyric_lines[index].time_ms : 0;
}

const char *crazypod_lyrics_plain_text(void)
{
    return lyric_status == CRAZYPOD_LYRICS_PLAIN
        ? lyric_text_pool : "";
}

static size_t plain_page_end(size_t start, size_t payload)
{
    size_t end = start + payload;

    if(end >= plain_text_length)
        return plain_text_length;
    while(end > start &&
          ((unsigned char)lyric_text_pool[end] & 0xc0) == 0x80)
        --end;
    return end;
}

static bool plain_page_bounds(
    int index, size_t capacity, size_t *start, size_t *end)
{
    size_t cursor = 0;
    int page = 0;

    if(index < 0 || capacity < 5 ||
       lyric_status != CRAZYPOD_LYRICS_PLAIN)
        return false;
    while(cursor < plain_text_length) {
        size_t next = plain_page_end(cursor, capacity - 1);

        if(next <= cursor)
            return false;
        if(page == index) {
            *start = cursor;
            *end = next;
            return true;
        }
        cursor = next;
        ++page;
    }
    return false;
}

int crazypod_lyrics_display_page_count(size_t capacity)
{
    size_t cursor = 0;
    int count = 0;

    if(lyric_status == CRAZYPOD_LYRICS_SYNCED)
        return lyric_count;
    if(lyric_status != CRAZYPOD_LYRICS_PLAIN || capacity < 5)
        return 0;
    while(cursor < plain_text_length) {
        size_t next = plain_page_end(cursor, capacity - 1);

        if(next <= cursor)
            return 0;
        cursor = next;
        ++count;
    }
    return count;
}

bool crazypod_lyrics_copy_display_page(
    int index, char *destination, size_t capacity)
{
    const char *source;
    size_t bytes;

    if(destination == NULL || capacity == 0)
        return false;
    destination[0] = '\0';
    if(lyric_status == CRAZYPOD_LYRICS_SYNCED) {
        if(index < 0 || index >= lyric_count)
            return false;
        source = crazypod_lyrics_line_text(index);
        bytes = lyric_lines[index].text_length;
        if(bytes >= capacity)
            return false;
    }
    else {
        size_t start;
        size_t end;

        if(!plain_page_bounds(index, capacity, &start, &end))
            return false;
        source = lyric_text_pool + start;
        bytes = end - start;
    }
    memcpy(destination, source, bytes);
    destination[bytes] = '\0';
    return true;
}

int crazypod_lyrics_current_line(uint32_t elapsed_ms)
{
    int lower = 0;
    int upper = lyric_count - 1;
    int result = -1;

    if(lyric_status != CRAZYPOD_LYRICS_SYNCED)
        return -1;
    while(lower <= upper) {
        int middle = lower + (upper - lower) / 2;

        if(lyric_lines[middle].time_ms <= elapsed_ms) {
            result = middle;
            lower = middle + 1;
        }
        else
            upper = middle - 1;
    }
    return result;
}

void crazypod_lyrics_window(
    uint32_t elapsed_ms, const char **previous,
    const char **current, const char **next)
{
    int index = crazypod_lyrics_current_line(elapsed_ms);

    if(lyric_status == CRAZYPOD_LYRICS_PLAIN) {
        if(previous != NULL)
            *previous = "";
        if(current != NULL)
            *current = lyric_text_pool;
        if(next != NULL)
            *next = "";
        return;
    }
    if(previous != NULL)
        *previous = index > 0
            ? crazypod_lyrics_line_text(index - 1) : "";
    if(current != NULL)
        *current = index >= 0
            ? crazypod_lyrics_line_text(index)
            : lyric_count > 0
                ? crazypod_lyrics_line_text(0) : "";
    if(next != NULL)
        *next = index >= 0
            ? index + 1 < lyric_count
                ? crazypod_lyrics_line_text(index + 1) : ""
            : lyric_count > 1
                ? crazypod_lyrics_line_text(1) : "";
}

#endif
