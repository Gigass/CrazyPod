#!/usr/bin/env python3
"""Run the production font loader/width lookup with an uncached bitmap stub."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "firmware/font.c").read_text()

def function(name):
    match = re.search(r"(?:static struct font\*|int)\s+" + name +
                      r"\([^)]*\)\s*\{.*?^\}", source, re.S | re.M)
    assert match, name
    return match.group()

harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define IPOD_6G
#define FONT_HEADER_SIZE 36
#define MAX_FONTSIZE_FOR_16_BIT_OFFSETS 0xffdb
#define SEEK_SET 0
typedef unsigned ucschar_t;
struct font_cache_entry { int width; };
struct font {
    unsigned char *buffer_start, *buffer_position, *buffer_end;
    const unsigned char *width, *bits;
    size_t buffer_size;
    int32_t bits_size;
    uint32_t file_offset_offset, file_width_offset;
    unsigned firstchar;
    int long_offset, fd, size, defaultchar, maxwidth;
    bool disabled;
    int cache;
};
static struct font sysfont;
static void load_cache_entry(struct font_cache_entry *entry, void *data)
{ (void)entry; (void)data; assert(!"width lookup loaded a bitmap"); }
static struct font_cache_entry *font_cache_get(
    int *cache, unsigned ch, bool cached_only,
    void (*load)(struct font_cache_entry *, void *), struct font *font)
{
    (void)cache; (void)ch; (void)cached_only; (void)load; (void)font;
    assert(!"width lookup touched bitmap cache");
    return NULL;
}
static void cache_create(struct font *font)
{ memset(font->buffer_start, 0xa5, font->buffer_size); }
'''
harness += function("font_load_cached") + "\n" + function("font_get_width")
harness += r'''
int main(void)
{
    unsigned char memory[68];
    unsigned char file[43] = {0};
    file[40] = 2; file[41] = 4; file[42] = 6;
    FILE *stream = tmpfile();
    assert(stream);
    assert(fwrite(file, 1, sizeof(file), stream) == sizeof(file));
    assert(fflush(stream) == 0);
    memset(memory, 0xcc, sizeof(memory));
    struct font font = {
        .buffer_start = memory, .buffer_position = memory + 36,
        .buffer_end = memory + 64, .buffer_size = 64,
        .bits_size = 4, .fd = fileno(stream),
        .firstchar = 65, .size = 3, .defaultchar = 66, .maxwidth = 6,
    };
    assert(font_load_cached(&font, 3, 0) == &font);
    assert(font.buffer_size == 64 && memory[63] == 0xa5);
    assert(font.width == memory + 64 && memory[67] == 0xcc);
    assert(fclose(stream) == 0);
    /* Descriptor remains nonnegative, but the file is now closed. Neither
     * valid nor fallback width lookups may try to fetch a bitmap. */
    for(int i = 0; i < 1000; ++i) {
        assert(font_get_width(&font, 65) == 2);
        assert(font_get_width(&font, 67) == 6);
        assert(font_get_width(&font, 0) == 4);
    }
    font.width = NULL;
    font.file_width_offset = 0;
    assert(font_get_width(&font, 65) == 6);
    /* Truncated width data must fail the font load. */
    stream = tmpfile();
    assert(stream);
    font.fd = fileno(stream);
    font.buffer_position = memory + 36;
    assert(font_load_cached(&font, 3, 0) == NULL);
    assert(fclose(stream) == 0);
    return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / "test.c").write_text(harness)
    subprocess.run(["cc", "-std=c99", "-D_POSIX_C_SOURCE=200809L", "-Wall",
                    "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    str(path / "test.c"), "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True)
print("Font widths: no bitmap I/O, bounded table, short-read failure passed")
