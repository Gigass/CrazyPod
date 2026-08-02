#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "file.h"

#include "crazypod_lyrics.h"

#define CRAZYPOD_LYRIC_LINE_COUNT 96
#define CRAZYPOD_LYRIC_TEXT_SIZE 128
#define CRAZYPOD_LRC_LINE_SIZE 384

struct crazypod_lyric_line {
    uint32_t time_ms;
    char text[CRAZYPOD_LYRIC_TEXT_SIZE];
};

static struct crazypod_lyric_line lyric_lines[CRAZYPOD_LYRIC_LINE_COUNT];
static int lyric_count;
static char loaded_track_path[MAX_PATH];

static int read_lrc_line(int fd, char *buffer, int size)
{
    int used = 0;
    char value;
    bool saw_data = false;

    if(size <= 1)
        return -1;
    while(read(fd, &value, 1) == 1) {
        saw_data = true;
        if(value == '\n')
            break;
        if(value == '\r')
            continue;
        if(used + 1 < size)
            buffer[used++] = value;
    }
    buffer[used] = '\0';
    return saw_data ? used : -1;
}

static bool parse_number(const char **cursor, const char *end,
                         unsigned *value)
{
    const char *text = *cursor;
    unsigned result = 0;
    bool found = false;

    while(text < end && *text >= '0' && *text <= '9') {
        result = result * 10 + (unsigned)(*text - '0');
        found = true;
        ++text;
    }
    if(!found)
        return false;
    *cursor = text;
    *value = result;
    return true;
}

static bool parse_timestamp(const char *open, const char *close,
                            uint32_t *time_ms)
{
    const char *cursor = open + 1;
    unsigned minutes;
    unsigned seconds;
    unsigned fraction = 0;
    unsigned fraction_digits = 0;

    if(!parse_number(&cursor, close, &minutes) ||
       cursor >= close || *cursor++ != ':' ||
       !parse_number(&cursor, close, &seconds))
        return false;
    if(cursor < close && (*cursor == '.' || *cursor == ':')) {
        ++cursor;
        while(cursor < close && *cursor >= '0' && *cursor <= '9' &&
              fraction_digits < 3) {
            fraction = fraction * 10 + (unsigned)(*cursor - '0');
            ++fraction_digits;
            ++cursor;
        }
    }
    if(cursor != close || seconds >= 60)
        return false;
    if(fraction_digits == 1)
        fraction *= 100;
    else if(fraction_digits == 2)
        fraction *= 10;
    *time_ms = (minutes * 60u + seconds) * 1000u + fraction;
    return true;
}

static void add_lyric_line(uint32_t time_ms, const char *text)
{
    struct crazypod_lyric_line line;
    size_t length;
    int insert_at;

    if(text == NULL || text[0] == '\0' ||
       lyric_count >= CRAZYPOD_LYRIC_LINE_COUNT)
        return;
    line.time_ms = time_ms;
    length = strlen(text);
    if(length >= sizeof(line.text))
        length = sizeof(line.text) - 1;
    while(length > 0 &&
          ((unsigned char)text[length] & 0xc0) == 0x80)
        --length;
    memcpy(line.text, text, length);
    line.text[length] = '\0';
    insert_at = lyric_count;
    while(insert_at > 0 &&
          lyric_lines[insert_at - 1].time_ms > time_ms) {
        lyric_lines[insert_at] = lyric_lines[insert_at - 1];
        --insert_at;
    }
    lyric_lines[insert_at] = line;
    ++lyric_count;
}

static void parse_lrc_line(char *line)
{
    uint32_t timestamps[4];
    int timestamp_count = 0;
    char *cursor = line;
    char *text;
    int i;

    while(*cursor == '[' && timestamp_count < 4) {
        char *close = strchr(cursor, ']');
        uint32_t time_ms;
        if(close == NULL ||
           !parse_timestamp(cursor, close, &time_ms))
            break;
        timestamps[timestamp_count++] = time_ms;
        cursor = close + 1;
    }
    if(timestamp_count <= 0)
        return;
    while(*cursor == ' ' || *cursor == '\t')
        ++cursor;
    text = cursor;
    for(i = 0; i < timestamp_count; ++i)
        add_lyric_line(timestamps[i], text);
}

static bool build_lrc_path(const char *track_path, char *path,
                           size_t size, bool uppercase)
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
    int fd;

    if(track_path != NULL &&
       strcmp(loaded_track_path, track_path) == 0)
        return lyric_count > 0;
    lyric_count = 0;
    loaded_track_path[0] = '\0';
    if(track_path != NULL)
        snprintf(loaded_track_path, sizeof(loaded_track_path),
                 "%s", track_path);
    if(!build_lrc_path(track_path, path, sizeof(path), false))
        return false;
    fd = open(path, O_RDONLY);
    if(fd < 0 &&
       build_lrc_path(track_path, path, sizeof(path), true))
        fd = open(path, O_RDONLY);
    if(fd < 0)
        return false;
    while(read_lrc_line(fd, line, sizeof(line)) >= 0) {
        if(line[0] != '\0')
            parse_lrc_line(line);
    }
    close(fd);
    return lyric_count > 0;
}

bool crazypod_lyrics_available(void)
{
    return lyric_count > 0;
}

int crazypod_lyrics_current_line(uint32_t elapsed_ms)
{
    int index = -1;
    int i;

    for(i = 0; i < lyric_count; ++i) {
        if(lyric_lines[i].time_ms > elapsed_ms)
            break;
        index = i;
    }
    return index;
}

void crazypod_lyrics_window(uint32_t elapsed_ms,
                            const char **previous,
                            const char **current,
                            const char **next)
{
    int index = crazypod_lyrics_current_line(elapsed_ms);
    if(previous != NULL)
        *previous = index > 0 ? lyric_lines[index - 1].text : "";
    if(current != NULL)
        *current = index >= 0 ? lyric_lines[index].text :
                   lyric_count > 0 ? lyric_lines[0].text : "";
    if(next != NULL)
        *next = index >= 0
            ? (index + 1 < lyric_count
                   ? lyric_lines[index + 1].text : "")
            : (lyric_count > 1 ? lyric_lines[1].text : "");
}

#endif
