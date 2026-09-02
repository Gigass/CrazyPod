#ifndef CRAZYPOD_EPUB_HTML_H
#define CRAZYPOD_EPUB_HTML_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CRAZYPOD_EPUB_IMAGE_MARKER '\x1f'
#define CRAZYPOD_EPUB_FORMAT_MARKER '\x1e'

/* The extracted EPUB stream keeps block style metadata next to the text.
 * These bytes are private to the reader and are ignored by plain-text
 * consumers.  Keeping the metadata in the cache lets pagination and display
 * agree without retaining a full XHTML DOM in firmware RAM. */
enum crazypod_epub_format_style {
    /* The style byte lives in a NUL-terminated cache stream.  Keep every
     * style value non-zero so NORMAL metadata cannot terminate the string. */
    CRAZYPOD_EPUB_FORMAT_NORMAL = 1,
    CRAZYPOD_EPUB_FORMAT_HEADING = 2,
    CRAZYPOD_EPUB_FORMAT_QUOTE = 3,
    CRAZYPOD_EPUB_FORMAT_PRE = 4,
    CRAZYPOD_EPUB_FORMAT_LIST = 5,
    CRAZYPOD_EPUB_FORMAT_CENTER = 0x80
};

typedef bool (*crazypod_epub_html_image_callback)(
    const char *source, uint32_t offset, void *context);

int crazypod_epub_html_decode_entity(const char *entity, char *output);
const char *crazypod_epub_html_find_ascii(const char *text,
                                          const char *needle);
bool crazypod_epub_html_copy_markup_text(char *output, size_t size,
                                         const char *start,
                                         const char *end);
bool crazypod_epub_html_append_text(const char *path, int output_fd);
bool crazypod_epub_html_append_text_with_images(
    const char *path, int output_fd,
    crazypod_epub_html_image_callback image_callback, void *context);
/* Like append_text_with_images, but preserves block-level reader styles in
 * the extracted stream for the on-device renderer. */
bool crazypod_epub_html_append_rich_text_with_images(
    const char *path, int output_fd,
    crazypod_epub_html_image_callback image_callback, void *context);

#endif
