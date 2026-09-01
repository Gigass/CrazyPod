#ifndef CRAZYPOD_EPUB_HTML_H
#define CRAZYPOD_EPUB_HTML_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CRAZYPOD_EPUB_IMAGE_MARKER '\x1f'

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

#endif
