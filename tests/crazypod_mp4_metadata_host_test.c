/*
 * The MP4 parser reads a track's tags and its duration out of the same
 * atom tree, and a case label in the wrong place once cost it both: the
 * meta atom stopped falling through to the recursion that reads the tag
 * list, so every title, artist, album and cover went missing, and the
 * bytes read in their place became a bogus duration.
 *
 * Build a small but structurally real MP4 and check that both halves
 * still come out, including the case the duration cross-check exists
 * for: a sample count that disagrees with the movie header.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "metadata.h"
#include "metadata_parsers.h"

/*
 * Not CHECK(): rbcodec's platform header leaves the host assert macro
 * bound to a glibc entry point that crashes while printing, so a failure
 * arrived as a segmentation fault with no message.
 */
#define CHECK(condition) \
    do { \
        if(!(condition)) { \
            fprintf(stderr, "%s:%d: failed: %s\n", \
                    __FILE__, __LINE__, #condition); \
            exit(1); \
        } \
    } while(0)

#define TIMESCALE 1000u
#define DURATION_MS 900000u
#define FREQUENCY 44100u

/* The pieces of Rockbox the parser calls into but this test does not
 * exercise. */
off_t filesize(int fd)
{
    off_t here = lseek(fd, 0, SEEK_CUR);
    off_t end = lseek(fd, 0, SEEK_END);

    lseek(fd, here, SEEK_SET);
    return end;
}

long parse_replaygain(const char *key, const char *value,
                      struct mp3entry *id3)
{
    (void)key;
    (void)value;
    (void)id3;
    return 0;
}

char *id3_get_num_genre(unsigned int genre_num)
{
    (void)genre_num;
    return NULL;
}

int getid3v2len(int fd)
{
    (void)fd;
    return 0;
}

static unsigned char image[8192];
static size_t image_len;

static void push(const void *data, size_t count)
{
    CHECK(image_len + count <= sizeof(image));
    memcpy(image + image_len, data, count);
    image_len += count;
}

static void push_u32(uint32_t value)
{
    unsigned char bytes[4];

    bytes[0] = (unsigned char)(value >> 24);
    bytes[1] = (unsigned char)(value >> 16);
    bytes[2] = (unsigned char)(value >> 8);
    bytes[3] = (unsigned char)value;
    push(bytes, sizeof(bytes));
}

static void push_zeros(size_t count)
{
    while(count-- > 0) {
        unsigned char zero = 0;

        push(&zero, 1);
    }
}

static size_t atom_begin_bytes(const char *type)
{
    size_t at = image_len;

    push_u32(0);
    push(type, 4);
    return at;
}

static size_t atom_begin(const char *type)
{
    return atom_begin_bytes(type);
}

static void atom_end(size_t at)
{
    uint32_t size = (uint32_t)(image_len - at);

    image[at] = (unsigned char)(size >> 24);
    image[at + 1] = (unsigned char)(size >> 16);
    image[at + 2] = (unsigned char)(size >> 8);
    image[at + 3] = (unsigned char)size;
}

/* An iTunes-style text tag: the four-character name wrapping a data atom
 * whose sixteen-byte header the parser skips. The three names used here
 * begin with the copyright sign, spelled out so no string literal has to
 * carry a byte above 0x7f next to an ASCII one. */
static void push_text_tag(const char *name, const char *text)
{
    char fourcc[4];
    size_t tag;
    size_t data;

    fourcc[0] = (char)0xa9;
    memcpy(fourcc + 1, name, 3);
    tag = atom_begin_bytes(fourcc);
    data = atom_begin("data");

    push_u32(1);        /* version and flags: UTF-8 */
    push_u32(0);        /* reserved */
    push(text, strlen(text));
    atom_end(data);
    atom_end(tag);
}

static void build(uint64_t samples)
{
    size_t moov, trak, mdia, minf, stbl, stsd, alac, inner;
    size_t stts, udta, meta, ilst, hdlr, mdat, ftyp;

    image_len = 0;

    ftyp = atom_begin("ftyp");
    push("M4A ", 4);
    push_u32(0);
    atom_end(ftyp);

    moov = atom_begin("moov");

    /* Movie header, version 0: the container's own clock. */
    {
        size_t mvhd = atom_begin("mvhd");

        push_u32(0);                    /* version and flags */
        push_u32(0);                    /* creation time */
        push_u32(0);                    /* modification time */
        push_u32(TIMESCALE);
        push_u32(DURATION_MS * TIMESCALE / 1000u);
        atom_end(mvhd);
    }

    trak = atom_begin("trak");
    mdia = atom_begin("mdia");

    hdlr = atom_begin("hdlr");
    push_zeros(8);
    push("soun", 4);
    atom_end(hdlr);

    minf = atom_begin("minf");
    stbl = atom_begin("stbl");

    stsd = atom_begin("stsd");
    push_u32(0);                        /* version and flags */
    push_u32(1);                        /* entry count */
    alac = atom_begin("alac");
    push_zeros(28);
    inner = atom_begin("alac");
    push_zeros(24);
    push_u32(FREQUENCY);
    atom_end(inner);
    atom_end(alac);
    atom_end(stsd);

    stts = atom_begin("stts");
    push_u32(0);                        /* version and flags */
    push_u32(1);                        /* entry count */
    push_u32(1);                        /* sample count */
    push_u32((uint32_t)samples);        /* duration per sample */
    atom_end(stts);

    atom_end(stbl);
    atom_end(minf);
    atom_end(mdia);
    atom_end(trak);

    udta = atom_begin("udta");
    meta = atom_begin("meta");
    push_u32(0);                        /* version and flags */

    hdlr = atom_begin("hdlr");
    push_zeros(8);
    push("mdir", 4);
    atom_end(hdlr);

    ilst = atom_begin("ilst");
    push_text_tag("nam", "Chapter One");
    push_text_tag("ART", "A Narrator");
    push_text_tag("alb", "A Book");
    atom_end(ilst);

    atom_end(meta);
    atom_end(udta);
    atom_end(moov);

    mdat = atom_begin("mdat");
    push_zeros(64);
    atom_end(mdat);
}

static void parse(struct mp3entry *id3)
{
    char path[] = "/tmp/crazypod-mp4-XXXXXX";
    int fd = mkstemp(path);

    CHECK(fd >= 0);
    CHECK(write(fd, image, image_len) == (ssize_t)image_len);
    CHECK(lseek(fd, 0, SEEK_SET) == 0);
    memset(id3, 0, sizeof(*id3));
    CHECK(get_mp4_metadata(fd, id3));
    close(fd);
    remove(path);
}

int main(void)
{
    struct mp3entry id3;

    /* A track whose sample count agrees with the movie header. */
    build((uint64_t)FREQUENCY * DURATION_MS / 1000u);
    parse(&id3);
    CHECK(id3.title != NULL && strcmp(id3.title, "Chapter One") == 0);
    CHECK(id3.artist != NULL && strcmp(id3.artist, "A Narrator") == 0);
    CHECK(id3.album != NULL && strcmp(id3.album, "A Book") == 0);
    CHECK(id3.frequency == FREQUENCY);
    CHECK(id3.length == (int)DURATION_MS);

    /*
     * Twice the samples for the same audio, which is what an SBR track
     * looks like on a build with the SBR decoder off. The tags still have
     * to arrive, and the movie header still has to win.
     */
    build((uint64_t)FREQUENCY * DURATION_MS * 2u / 1000u);
    parse(&id3);
    CHECK(id3.title != NULL && strcmp(id3.title, "Chapter One") == 0);
    CHECK(id3.length == (int)DURATION_MS);

    puts("MP4 metadata: tags and movie-header duration parse together");
    return 0;
}
