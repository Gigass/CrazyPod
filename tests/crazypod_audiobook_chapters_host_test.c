#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "crazypod_audiobook_chapters.h"

struct buffer {
    unsigned char data[1024];
    uint32_t size;
};

static bool read_buffer(void *context, uint32_t offset, void *out,
                        uint32_t size)
{
    const struct buffer *buffer = context;

    if(offset > buffer->size || size > buffer->size - offset)
        return false;
    memcpy(out, buffer->data + offset, size);
    return true;
}

static uint32_t put32(struct buffer *b, uint32_t at, uint32_t value)
{
    b->data[at] = (unsigned char)(value >> 24);
    b->data[at + 1] = (unsigned char)(value >> 16);
    b->data[at + 2] = (unsigned char)(value >> 8);
    b->data[at + 3] = (unsigned char)value;
    return at + 4;
}

static uint32_t put_type(struct buffer *b, uint32_t at, const char *type)
{
    memcpy(b->data + at, type, 4);
    return at + 4;
}

/* Appends a chpl entry at "at" and returns the next offset. */
static uint32_t put_chapter(struct buffer *b, uint32_t at,
                            uint32_t start_ms, const char *title)
{
    uint64_t units = (uint64_t)start_ms * 10000u;
    size_t length = strlen(title);

    at = put32(b, at, (uint32_t)(units >> 32));
    at = put32(b, at, (uint32_t)units);
    b->data[at++] = (unsigned char)length;
    memcpy(b->data + at, title, length);
    return at + (uint32_t)length;
}

/*
 * Lays out: ftyp, a 64-bit-sized mdat, then moov{ mvhd, udta{ chpl } }
 * so the walker has to cope with largesize, unrelated siblings and a moov
 * that follows the media data as ffmpeg writes it without faststart.
 */
static void build_file(struct buffer *b, int chapter_count,
                       bool with_chpl)
{
    uint32_t at = 0;
    uint32_t moov_at;
    uint32_t udta_at;
    uint32_t chpl_at;
    int i;

    memset(b, 0, sizeof(*b));
    at = put32(b, at, 16);
    at = put_type(b, at, "ftyp");
    at = put_type(b, at, "M4B ");
    at = put32(b, at, 0);
    /* mdat with a 64-bit size of 40 bytes total */
    at = put32(b, at, 1);
    at = put_type(b, at, "mdat");
    at = put32(b, at, 0);
    at = put32(b, at, 40);
    at += 24;
    moov_at = at;
    at += 8;
    at = put32(b, at, 12);
    at = put_type(b, at, "mvhd");
    at = put32(b, at, 0);
    udta_at = at;
    at += 8;
    if(with_chpl) {
        chpl_at = at;
        at += 8;
        at = put32(b, at, 0x01000000u);
        at = put32(b, at, 0);
        b->data[at++] = (unsigned char)chapter_count;
        for(i = 0; i < chapter_count; ++i) {
            char title[32];

            snprintf(title, sizeof(title), "Part %d", i + 1);
            at = put_chapter(b, at, (uint32_t)i * 90000u + 500u,
                             i == 1 ? "" : title);
        }
        put32(b, chpl_at, at - chpl_at);
        put_type(b, chpl_at + 4, "chpl");
    }
    put32(b, udta_at, at - udta_at);
    put_type(b, udta_at + 4, "udta");
    put32(b, moov_at, at - moov_at);
    put_type(b, moov_at + 4, "moov");
    b->size = at;
}

int main(void)
{
    struct buffer file;
    struct crazypod_audiobook_chapter chapters[8];
    int count;

    build_file(&file, 3, true);
    count = crazypod_audiobook_parse_chapters(
        read_buffer, &file, file.size, chapters, 8);
    assert(count == 3);
    assert(chapters[0].start_ms == 500);
    assert(strcmp(chapters[0].title, "Part 1") == 0);
    /* An empty title gets a generated one. */
    assert(strcmp(chapters[1].title, "Chapter 2") == 0);
    assert(chapters[2].start_ms == 180500);
    assert(strcmp(chapters[2].title, "Part 3") == 0);

    /* max_chapters caps the table without reading past it. */
    count = crazypod_audiobook_parse_chapters(
        read_buffer, &file, file.size, chapters, 2);
    assert(count == 2);

    /* No chpl atom: zero chapters, not an error. */
    build_file(&file, 0, false);
    count = crazypod_audiobook_parse_chapters(
        read_buffer, &file, file.size, chapters, 8);
    assert(count == 0);

    /* A file truncated inside the chapter list is an error. */
    build_file(&file, 3, true);
    count = crazypod_audiobook_parse_chapters(
        read_buffer, &file, file.size - 4, chapters, 8);
    assert(count == -1);

    /* Not an MP4 at all: no moov, so no chapters. */
    memset(&file, 0, sizeof(file));
    file.size = 64;
    count = crazypod_audiobook_parse_chapters(
        read_buffer, &file, file.size, chapters, 8);
    assert(count <= 0);

    printf("crazypod_audiobook_chapters_host_test: ok\n");
    return 0;
}
