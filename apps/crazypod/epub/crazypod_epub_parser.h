#ifndef CRAZYPOD_EPUB_PARSER_H
#define CRAZYPOD_EPUB_PARSER_H

#include <stdbool.h>
#include <stddef.h>

bool crazypod_epub_join_path(
    char *output, size_t size, const char *left, const char *right);
int crazypod_epub_ascii_lower(int value);
bool crazypod_epub_ascii_equal(
    const char *left, const char *right);
const char *crazypod_epub_find_start_tag(
    const char *cursor, const char *local_name);
bool crazypod_epub_token_contains(
    const char *tokens, const char *wanted);
bool crazypod_epub_resolve_href(
    char *output, size_t size, const char *root,
    const char *directory, const char *href);
bool crazypod_epub_extract_attribute(
    const char *start, const char *end, const char *name,
    char *output, size_t size);
void crazypod_epub_directory_of(
    char *directory, size_t size, const char *path);

#endif
