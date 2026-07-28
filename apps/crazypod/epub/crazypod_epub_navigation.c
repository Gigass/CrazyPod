#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "crazypod_epub_html.h"
#include "crazypod_epub_navigation.h"
#include "crazypod_epub_parser.h"

int crazypod_epub_navigation_parse_epub3(
    const char *xml, struct crazypod_epub_navigation_entry *entries,
    int capacity)
{
    const char *nav = xml;

    while((nav = crazypod_epub_find_start_tag(nav, "nav")) != NULL) {
        const char *nav_open_end = strchr(nav, '>');
        const char *nav_end;
        const char *cursor;
        char type[96];
        int count = 0;

        if(nav_open_end == NULL)
            return 0;
        type[0] = '\0';
        crazypod_epub_extract_attribute(
            nav, nav_open_end, "epub:type", type, sizeof(type));
        if(type[0] == '\0')
            crazypod_epub_extract_attribute(
                nav, nav_open_end, "role", type, sizeof(type));
        nav_end = crazypod_epub_html_find_ascii(nav_open_end, "</nav");
        if(nav_end == NULL)
            nav_end = xml + strlen(xml);
        if(!crazypod_epub_token_contains(type, "toc") &&
           !crazypod_epub_token_contains(type, "doc-toc")) {
            nav = nav_open_end + 1;
            continue;
        }

        cursor = nav_open_end + 1;
        while(count < capacity &&
              (cursor = crazypod_epub_find_start_tag(cursor, "a")) != NULL &&
              cursor < nav_end) {
            const char *open_end = strchr(cursor, '>');
            const char *close;
            struct crazypod_epub_navigation_entry *entry = &entries[count];

            if(open_end == NULL || open_end >= nav_end)
                break;
            close = crazypod_epub_html_find_ascii(open_end, "</a");
            if(close == NULL || close > nav_end)
                break;
            entry->href[0] = '\0';
            entry->title[0] = '\0';
            if(crazypod_epub_extract_attribute(
                   cursor, open_end, "href",
                   entry->href, sizeof(entry->href)) &&
               crazypod_epub_html_copy_markup_text(
                   entry->title, sizeof(entry->title),
                   open_end + 1, close))
                ++count;
            cursor = close + 1;
        }
        return count;
    }
    return 0;
}

int crazypod_epub_navigation_parse_ncx(
    const char *xml, struct crazypod_epub_navigation_entry *entries,
    int capacity)
{
    const char *cursor = xml;
    int count = 0;

    while(count < capacity &&
          (cursor = crazypod_epub_find_start_tag(
               cursor, "navPoint")) != NULL) {
        const char *point_end =
            crazypod_epub_html_find_ascii(cursor, "</navPoint");
        const char *content =
            crazypod_epub_find_start_tag(cursor, "content");
        const char *text = crazypod_epub_find_start_tag(cursor, "text");
        const char *content_end;
        const char *text_start;
        const char *text_end;
        struct crazypod_epub_navigation_entry *entry = &entries[count];

        if(point_end == NULL)
            point_end = xml + strlen(xml);
        if(content == NULL || content >= point_end ||
           text == NULL || text >= point_end ||
           (content_end = strchr(content, '>')) == NULL ||
           (text_start = strchr(text, '>')) == NULL) {
            ++cursor;
            continue;
        }
        ++text_start;
        text_end = strchr(text_start, '<');
        entry->href[0] = '\0';
        entry->title[0] = '\0';
        if(text_end != NULL && text_end < point_end &&
           crazypod_epub_extract_attribute(
               content, content_end, "src",
               entry->href, sizeof(entry->href)) &&
           crazypod_epub_html_copy_markup_text(
               entry->title, sizeof(entry->title),
               text_start, text_end))
            ++count;
        ++cursor;
    }
    return count;
}

#endif
