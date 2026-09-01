#ifndef CRAZYPOD_EPUB_LAYOUT_H
#define CRAZYPOD_EPUB_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>

/* Paginate a UTF-8 view while returning the byte count consumed from source.
 * source may be a GBK buffer when source_is_gbk is true; this keeps saved
 * reading offsets aligned with the original file instead of the converted
 * UTF-8 buffer. */
size_t crazypod_epub_layout_page(
    const unsigned char *utf8, size_t utf8_count,
    const unsigned char *source, size_t source_count,
    bool source_is_gbk, bool markdown,
    char *output, size_t output_size,
    unsigned max_lines, unsigned max_line_units);

#endif
