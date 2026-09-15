#ifndef CRAZYPOD_AUDIOBOOK_CHAPTERS_H
#define CRAZYPOD_AUDIOBOOK_CHAPTERS_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Chapter table of an MP4 audiobook, read from the Nero "chpl" atom that
 * ffmpeg, m4b-tool and most converters write under moov/udta. Pure code:
 * the file is reached through a read callback so host tests can feed it a
 * buffer.
 */

#define CRAZYPOD_AUDIOBOOK_CHAPTERS_MAX 255
#define CRAZYPOD_AUDIOBOOK_CHAPTER_TITLE_SIZE 64

struct crazypod_audiobook_chapter {
    uint32_t start_ms;
    char title[CRAZYPOD_AUDIOBOOK_CHAPTER_TITLE_SIZE];
};

/* Reads size bytes at offset into buffer; false on a short read. */
typedef bool (*crazypod_audiobook_read_fn)(
    void *context, uint32_t offset, void *buffer, uint32_t size);

/* Returns the chapter count, 0 when the file carries no chpl atom, or -1
 * when the atom structure cannot be read. */
int crazypod_audiobook_parse_chapters(
    crazypod_audiobook_read_fn read, void *context, uint32_t file_size,
    struct crazypod_audiobook_chapter *chapters, int max_chapters);

#endif
