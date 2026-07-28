#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "dir.h"
#include "file.h"
#include "zip.h"

#include "../crazypod_epub_parser.h"
#include "crazypod_epub_extraction.h"

struct epub_extract_filter {
    const char (*entries)[MAX_PATH];
    int count;
};

static int extract_filter_callback(
    const struct zip_args *args, int pass, void *context)
{
    const struct epub_extract_filter *filter = context;
    int i;

    if(pass != ZIP_PASS_START)
        return 0;
    for(i = 0; i < filter->count; ++i) {
        if(strcmp(args->name, filter->entries[i]) == 0)
            return 0;
    }
    return -1;
}

bool crazypod_epub_extraction_remove_tree(const char *path)
{
    while(true) {
        DIR *directory = opendir(path);
        struct DIRENT *entry;
        char child[MAX_PATH];
        bool found = false;
        bool is_directory = false;

        if(directory == NULL)
            return remove(path) == 0;
        while((entry = readdir(directory)) != NULL) {
            struct dirinfo info;

            if(strcmp(entry->d_name, ".") == 0 ||
               strcmp(entry->d_name, "..") == 0 ||
               !crazypod_epub_join_path(
                   child, sizeof(child), path, entry->d_name))
                continue;
            info = dir_get_info(directory, entry);
            is_directory = (info.attribute & ATTR_DIRECTORY) != 0;
            found = true;
            break;
        }
        closedir(directory);
        if(!found)
            return rmdir(path) == 0;

        /*
         * Do not mutate a FAT directory while iterating it. Its cursor can
         * otherwise return the deleted entry again and stall cleanup.
         */
        if(is_directory) {
            if(!crazypod_epub_extraction_remove_tree(child))
                return false;
        }
        else if(remove(child) < 0)
            return false;
    }
}

bool crazypod_epub_extraction_archive_name(
    char *output, size_t size, const char *root, const char *path)
{
    size_t root_length = strlen(root);
    int result;

    if(strncmp(path, root, root_length) != 0 ||
       path[root_length] != '/')
        return false;
    result = snprintf(output, size, "%s", path + root_length + 1);
    return result >= 0 && (size_t)result < size;
}

bool crazypod_epub_extraction_entries(
    const char *epub_path, const char *extract_root,
    const char entries[][MAX_PATH], int entry_count)
{
    struct epub_extract_filter filter;
    struct zip *archive;
    int result;

    if(entry_count <= 0)
        return false;
    filter.entries = entries;
    filter.count = entry_count;
    archive = zip_open(epub_path, false);
    if(archive == NULL)
        return false;
    result = zip_extract(
        archive, extract_root, extract_filter_callback, &filter);
    zip_close(archive);
    return result == 0;
}

bool crazypod_epub_extraction_entry(
    const char *epub_path, const char *extract_root, const char *entry)
{
    char single_entry[1][MAX_PATH];
    int result = snprintf(
        single_entry[0], sizeof(single_entry[0]), "%s", entry);

    return result >= 0 &&
        (size_t)result < sizeof(single_entry[0]) &&
        crazypod_epub_extraction_entries(
            epub_path, extract_root, single_entry, 1);
}

#endif
