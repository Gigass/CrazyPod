#ifndef CRAZYPOD_EPUB_HTML_H
#define CRAZYPOD_EPUB_HTML_H

#include <stdbool.h>
#include <stddef.h>

int crazypod_epub_html_decode_entity(const char *entity, char *output);
const char *crazypod_epub_html_find_ascii(const char *text,
                                          const char *needle);
bool crazypod_epub_html_copy_markup_text(char *output, size_t size,
                                         const char *start,
                                         const char *end);
bool crazypod_epub_html_append_text(const char *path, int output_fd);

#endif
