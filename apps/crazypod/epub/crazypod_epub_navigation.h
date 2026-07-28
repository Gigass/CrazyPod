#ifndef CRAZYPOD_EPUB_NAVIGATION_H
#define CRAZYPOD_EPUB_NAVIGATION_H

struct crazypod_epub_navigation_entry {
    char href[192];
    char title[96];
};

int crazypod_epub_navigation_parse_epub3(
    const char *xml, struct crazypod_epub_navigation_entry *entries,
    int capacity);
int crazypod_epub_navigation_parse_ncx(
    const char *xml, struct crazypod_epub_navigation_entry *entries,
    int capacity);

#endif
