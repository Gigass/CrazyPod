#ifndef CRAZYPOD_EPUB_LAYOUT_H
#define CRAZYPOD_EPUB_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef unsigned (*crazypod_epub_layout_width_fn)(
    uint32_t codepoint, uint32_t next_codepoint, void *context);

enum crazypod_epub_source_encoding {
    CRAZYPOD_EPUB_SOURCE_UTF8,
    CRAZYPOD_EPUB_SOURCE_GBK,
    CRAZYPOD_EPUB_SOURCE_UTF16LE,
    CRAZYPOD_EPUB_SOURCE_UTF16BE,
};

/* The UTF-8 view may be converted from GBK or UTF-16.  source_count and the
 * returned offset always refer to the original byte buffer. */
size_t crazypod_epub_layout_page_with_encoding(
    const unsigned char *utf8, size_t utf8_count,
    const unsigned char *source, size_t source_count,
    enum crazypod_epub_source_encoding encoding, bool markdown,
    char *output, size_t output_size,
    unsigned max_lines, unsigned max_line_width);

size_t crazypod_epub_layout_page_with_measure_encoding(
    const unsigned char *utf8, size_t utf8_count,
    const unsigned char *source, size_t source_count,
    enum crazypod_epub_source_encoding encoding, bool markdown,
    char *output, size_t output_size,
    unsigned max_lines, unsigned max_line_width,
    crazypod_epub_layout_width_fn measure_width, void *context);

/* Paginate a UTF-8 view while returning the byte count consumed from source.
 * source may be a GBK buffer when source_is_gbk is true; this keeps saved
 * reading offsets aligned with the original file instead of the converted
 * UTF-8 buffer.  EPUB format markers are copied to output, but do not consume
 * display width. */
size_t crazypod_epub_layout_page(
    const unsigned char *utf8, size_t utf8_count,
    const unsigned char *source, size_t source_count,
    bool source_is_gbk, bool markdown,
    char *output, size_t output_size,
    unsigned max_lines, unsigned max_line_units);

/* Paginate using the rendered font's advance width in pixels.  The callback
 * may return zero for a missing glyph; the built-in Unicode width fallback is
 * then used so pagination remains safe before a runtime font is ready. */
size_t crazypod_epub_layout_page_with_measure(
    const unsigned char *utf8, size_t utf8_count,
    const unsigned char *source, size_t source_count,
    bool source_is_gbk, bool markdown,
    char *output, size_t output_size,
    unsigned max_lines, unsigned max_line_width,
    crazypod_epub_layout_width_fn measure_width, void *context);

#endif
