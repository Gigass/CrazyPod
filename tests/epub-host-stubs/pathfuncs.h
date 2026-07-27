#ifndef EPUB_HOST_TEST_PATHFUNCS_H
#define EPUB_HOST_TEST_PATHFUNCS_H

#include <stddef.h>
#include <string.h>

#define PATH_ROOTSTR "/"
#define PATH_SEPSTR "/"
#define PATH_SEPCH '/'

static inline size_t path_dirname(const char *path, const char **directory)
{
    const char *slash = strrchr(path, '/');
    *directory = path;
    return slash != NULL ? (size_t)(slash - path) : 0;
}

static inline size_t parse_path_component(
    const char **path, const char **name)
{
    const char *cursor = *path;
    const char *end;

    while(*cursor == '/')
        ++cursor;
    if(*cursor == '\0')
        return 0;
    *name = cursor;
    end = strchr(cursor, '/');
    if(end == NULL) {
        *path = cursor + strlen(cursor);
        return strlen(cursor);
    }
    *path = end;
    return (size_t)(end - cursor);
}

#endif
