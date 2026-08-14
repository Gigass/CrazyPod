#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "crazypod_lyrics.h"

static void write_text(const char *path, const char *text)
{
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    size_t length = strlen(text);

    assert(fd >= 0);
    assert(write(fd, text, length) == (ssize_t)length);
    assert(close(fd) == 0);
}

static void make_paths(
    const char *name, char *track, size_t track_size,
    char *lyrics, size_t lyrics_size)
{
    snprintf(track, track_size, "/tmp/%s-%ld.mp3", name,
             (long)getpid());
    snprintf(lyrics, lyrics_size, "/tmp/%s-%ld.lrc", name,
             (long)getpid());
}

static void test_synced_long_utf8_and_offset(void)
{
    char track[256];
    char lyrics[256];
    const char *long_line =
        "这是一句明显超过旧版四十二个汉字限制的歌词，用来确认完整的"
        "UTF-8内容会进入共享文本池而不是被静默截断。";
    char document[1024];

    make_paths("crazypod-synced", track, sizeof(track),
               lyrics, sizeof(lyrics));
    snprintf(document, sizeof(document),
             "[offset:+250]\n[00:01.00]%s\n"
             "[00:03.50]second line\n[00:05.00]\n",
             long_line);
    write_text(lyrics, document);

    assert(crazypod_lyrics_load(track));
    assert(crazypod_lyrics_get_status() == CRAZYPOD_LYRICS_SYNCED);
    assert(crazypod_lyrics_synchronized());
    assert(crazypod_lyrics_line_count() == 3);
    assert(strcmp(crazypod_lyrics_line_text(0), long_line) == 0);
    assert(crazypod_lyrics_line_time(0) == 1250);
    assert(crazypod_lyrics_current_line(1249) == -1);
    assert(crazypod_lyrics_current_line(1250) == 0);
    assert(crazypod_lyrics_current_line(4000) == 1);
    assert(strcmp(crazypod_lyrics_line_text(2), "") == 0);

    unlink(lyrics);
}

static void test_plain_lyrics(void)
{
    char track[256];
    char lyrics[256];
    char page[20];
    char reconstructed[128] = "";
    int index;

    make_paths("crazypod-plain", track, sizeof(track),
               lyrics, sizeof(lyrics));
    write_text(lyrics,
               "[ar:Artist]\nFirst plain line\nSecond plain line\n");

    assert(crazypod_lyrics_load(track));
    assert(crazypod_lyrics_get_status() == CRAZYPOD_LYRICS_PLAIN);
    assert(!crazypod_lyrics_synchronized());
    assert(strcmp(crazypod_lyrics_plain_text(),
                  "First plain line\nSecond plain line") == 0);
    assert(crazypod_lyrics_display_page_count(sizeof(page)) > 1);
    for(index = 0;
        index < crazypod_lyrics_display_page_count(sizeof(page));
        ++index) {
        assert(crazypod_lyrics_copy_display_page(
            index, page, sizeof(page)));
        strncat(reconstructed, page,
                sizeof(reconstructed) - strlen(reconstructed) - 1);
    }
    assert(strcmp(reconstructed, crazypod_lyrics_plain_text()) == 0);

    unlink(lyrics);
}

static void test_capacity_failure_is_not_partial(void)
{
    char track[256];
    char lyrics[256];
    int fd;
    int index;

    make_paths("crazypod-capacity", track, sizeof(track),
               lyrics, sizeof(lyrics));
    fd = open(lyrics, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    assert(fd >= 0);
    for(index = 0; index < 193; ++index) {
        char line[64];
        int length = snprintf(
            line, sizeof(line), "[%02d:%02d.00]line %d\n",
            index / 60, index % 60, index);

        assert(write(fd, line, (size_t)length) == length);
    }
    assert(close(fd) == 0);

    assert(!crazypod_lyrics_load(track));
    assert(crazypod_lyrics_get_status() == CRAZYPOD_LYRICS_CAPACITY);
    assert(!crazypod_lyrics_available());
    assert(crazypod_lyrics_line_count() == 0);

    unlink(lyrics);
}

static void test_invalid_utf8_is_rejected(void)
{
    char track[256];
    char lyrics[256];
    const char invalid[] = "[00:01.00]bad \xed\xa0\x80\n";

    make_paths("crazypod-invalid", track, sizeof(track),
               lyrics, sizeof(lyrics));
    write_text(lyrics, invalid);
    assert(!crazypod_lyrics_load(track));
    assert(crazypod_lyrics_get_status() == CRAZYPOD_LYRICS_INVALID);
    assert(!crazypod_lyrics_available());
    unlink(lyrics);
}

int main(void)
{
    test_synced_long_utf8_and_offset();
    test_plain_lyrics();
    test_capacity_failure_is_not_partial();
    test_invalid_utf8_is_rejected();
    puts("crazypod lyrics host tests passed");
    return 0;
}
