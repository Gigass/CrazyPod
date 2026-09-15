#include <stdio.h>
#include <string.h>

#include "crazypod_audiobook_chapters.h"

/* ---- Pure chapter parser ------------------------------------------------ */

#define FOURCC(a, b, c, d) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
     ((uint32_t)(c) << 8) | (uint32_t)(d))

static uint32_t be32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

/*
 * Finds a child atom by type between start and end. Handles the 64-bit
 * "largesize" form used by big mdat atoms and the size-0 "to end of file"
 * form. Returns the payload range through payload_start/payload_end.
 */
static int find_atom(
    crazypod_audiobook_read_fn read, void *context,
    uint32_t start, uint32_t end, uint32_t wanted,
    uint32_t *payload_start, uint32_t *payload_end)
{
    uint32_t offset = start;

    while(offset + 8 <= end) {
        uint8_t header[16];
        uint32_t size;
        uint32_t type;
        uint32_t header_size = 8;

        if(!read(context, offset, header, 8))
            return -1;
        size = be32(header);
        type = be32(header + 4);
        if(size == 1) {
            if(!read(context, offset + 8, header + 8, 8))
                return -1;
            /* Files this port can hold fit in 32 bits; refuse the rest. */
            if(be32(header + 8) != 0)
                return -1;
            size = be32(header + 12);
            header_size = 16;
        }
        else if(size == 0)
            size = end - offset;
        if(size < header_size || offset + size > end ||
           offset + size < offset)
            return -1;
        if(type == wanted) {
            *payload_start = offset + header_size;
            *payload_end = offset + size;
            return 1;
        }
        offset += size;
    }
    return 0;
}

int crazypod_audiobook_parse_chapters(
    crazypod_audiobook_read_fn read, void *context, uint32_t file_size,
    struct crazypod_audiobook_chapter *out, int max_chapters)
{
    uint32_t moov_start;
    uint32_t moov_end;
    uint32_t udta_start;
    uint32_t udta_end;
    uint32_t chpl_start;
    uint32_t chpl_end;
    uint32_t offset;
    uint8_t header[9];
    int count;
    int index;
    int found;

    found = find_atom(read, context, 0, file_size,
                      FOURCC('m', 'o', 'o', 'v'),
                      &moov_start, &moov_end);
    if(found <= 0)
        return found;
    found = find_atom(read, context, moov_start, moov_end,
                      FOURCC('u', 'd', 't', 'a'),
                      &udta_start, &udta_end);
    if(found <= 0)
        return found;
    found = find_atom(read, context, udta_start, udta_end,
                      FOURCC('c', 'h', 'p', 'l'),
                      &chpl_start, &chpl_end);
    if(found <= 0)
        return found;

    /* version(1) flags(3) reserved(4) count(1), then per chapter a 64-bit
     * start in 100 ns units, a length byte and the title. */
    if(chpl_end - chpl_start < 9 || !read(context, chpl_start, header, 9))
        return -1;
    count = header[8];
    offset = chpl_start + 9;
    for(index = 0; index < count && index < max_chapters; ++index) {
        uint8_t entry[9];
        uint32_t high;
        uint32_t low;
        uint32_t title_length;
        struct crazypod_audiobook_chapter *chapter = &out[index];

        if(offset + 9 > chpl_end || !read(context, offset, entry, 9))
            return -1;
        high = be32(entry);
        low = be32(entry + 4);
        /* 100 ns units to ms: divide by 10000, done in two halves so
         * seven-figure-hour books stay in 32-bit arithmetic. */
        chapter->start_ms =
            (uint32_t)((((uint64_t)high << 32) | low) / 10000u);
        title_length = entry[8];
        offset += 9;
        if(offset + title_length > chpl_end)
            return -1;
        {
            uint32_t copy = title_length;

            if(copy > sizeof(chapter->title) - 1)
                copy = sizeof(chapter->title) - 1;
            if(copy > 0 && !read(context, offset, chapter->title, copy))
                return -1;
            chapter->title[copy] = '\0';
        }
        if(chapter->title[0] == '\0')
            snprintf(chapter->title, sizeof(chapter->title),
                     "Chapter %d", index + 1);
        offset += title_length;
    }
    return index;
}
