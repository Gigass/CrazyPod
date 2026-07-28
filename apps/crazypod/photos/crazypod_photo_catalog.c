#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "dir.h"
#include "file.h"
#include "kernel.h"

#include "../crazypod_photos.h"
#include "crazypod_photo_catalog.h"

#define PHOTO_DIRECTORY "/Pictures"
#define PHOTO_DIRECTORY_DEPTH 4
#define PHOTO_STATE_DIRECTORY "/.crazypod"
#define PHOTO_FAVORITES_PATH PHOTO_STATE_DIRECTORY "/photo-favorites.cfg"
#define PHOTO_FAVORITES_TMP PHOTO_STATE_DIRECTORY "/photo-favorites.tmp"

static struct crazypod_photo_catalog_entry
    entries[CRAZYPOD_PHOTO_MAX_FILES];
static int entry_count;
static int favorites;

static bool write_exact(int fd, const void *data, size_t size)
{
    const uint8_t *cursor = data;

    while(size > 0) {
        ssize_t count = write(fd, cursor, size);

        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

uint32_t crazypod_photo_catalog_key(const char *path)
{
    uint32_t hash = 2166136261u;

    while(path != NULL && *path != '\0') {
        hash ^= (uint8_t)*path++;
        hash *= 16777619u;
    }
    return hash;
}

bool crazypod_photo_catalog_path_supported(const char *path)
{
    const char *extension = path != NULL ? strrchr(path, '.') : NULL;

    return extension != NULL &&
        (strcasecmp(extension, ".jpg") == 0 ||
         strcasecmp(extension, ".jpeg") == 0 ||
         strcasecmp(extension, ".bmp") == 0);
}

static bool directory_supported(const char *name)
{
    const char *extension = name != NULL ? strrchr(name, '.') : NULL;
    size_t length = name != NULL ? strlen(name) : 0;

    if(name == NULL || name[0] == '.')
        return false;
    if(extension != NULL &&
       (strcasecmp(extension, ".photoslibrary") == 0 ||
        strcasecmp(extension, ".photolibrary") == 0 ||
        strcasecmp(extension, ".app") == 0))
        return false;
    return length < 6 ||
        strcasecmp(name + length - 6, "_files") != 0;
}

static bool append_path(char *destination, size_t size,
                        const char *directory, const char *name)
{
    size_t directory_length = strlen(directory);
    size_t name_length = strlen(name);

    if(directory_length + 1 + name_length >= size)
        return false;
    memcpy(destination, directory, directory_length);
    destination[directory_length] = '/';
    memcpy(destination + directory_length + 1, name, name_length + 1);
    return true;
}

static int compare_entries(
    const struct crazypod_photo_catalog_entry *left,
    const struct crazypod_photo_catalog_entry *right)
{
    if(left->mtime != right->mtime)
        return left->mtime > right->mtime ? -1 : 1;
    return strcasecmp(left->path, right->path);
}

static void insert_entry(const char *path, const struct dirinfo *info)
{
    struct crazypod_photo_catalog_entry entry;
    int position;

    if(entry_count >= CRAZYPOD_PHOTO_MAX_FILES)
        return;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.path, sizeof(entry.path), "%s", path);
    entry.size = info->size <= 0 ? 0 :
        (uint64_t)info->size > UINT32_MAX ? UINT32_MAX :
        (uint32_t)info->size;
    entry.mtime = info->mtime <= 0 ? 0 :
        (uint64_t)info->mtime > UINT32_MAX ? UINT32_MAX :
        (uint32_t)info->mtime;
    entry.key = crazypod_photo_catalog_key(entry.path);
    position = entry_count;
    while(position > 0 &&
          compare_entries(&entry, &entries[position - 1]) < 0) {
        entries[position] = entries[position - 1];
        --position;
    }
    entries[position] = entry;
    ++entry_count;
}

static void scan_directory(const char *path, int depth)
{
    DIR *directory;
    struct DIRENT *entry;

    if(depth > PHOTO_DIRECTORY_DEPTH ||
       entry_count >= CRAZYPOD_PHOTO_MAX_FILES)
        return;
    directory = opendir(path);
    if(directory == NULL)
        return;
    while(entry_count < CRAZYPOD_PHOTO_MAX_FILES &&
          (entry = readdir(directory)) != NULL) {
        struct dirinfo info;
        char child[MAX_PATH];

        if(strcmp(entry->d_name, ".") == 0 ||
           strcmp(entry->d_name, "..") == 0 ||
           entry->d_name[0] == '.')
            continue;
        if(!append_path(child, sizeof(child), path, entry->d_name))
            continue;
        info = dir_get_info(directory, entry);
        if(info.attribute & ATTR_DIRECTORY) {
            if(directory_supported(entry->d_name))
                scan_directory(child, depth + 1);
        }
        else if(crazypod_photo_catalog_path_supported(child))
            insert_entry(child, &info);
        if((entry_count & 15) == 0)
            yield();
    }
    closedir(directory);
}

static void load_favorites(void)
{
    char line[MAX_PATH];
    int fd;
    int used = 0;

    favorites = 0;
    fd = open(PHOTO_FAVORITES_PATH, O_RDONLY);
    if(fd < 0)
        return;
    while(true) {
        char character;
        ssize_t count = read(fd, &character, 1);

        if(count <= 0)
            character = '\n';
        if(character == '\r')
            continue;
        if(character != '\n' && used < MAX_PATH - 1) {
            line[used++] = character;
            continue;
        }
        if(used > 0) {
            int index;

            line[used] = '\0';
            for(index = 0; index < entry_count; ++index) {
                if(strcmp(entries[index].path, line) == 0) {
                    if(!entries[index].favorite) {
                        entries[index].favorite = true;
                        ++favorites;
                    }
                    break;
                }
            }
        }
        used = 0;
        if(count <= 0)
            break;
    }
    close(fd);
}

static bool save_favorites(void)
{
    bool complete = true;
    int fd;
    int index;

    mkdir(PHOTO_STATE_DIRECTORY);
    fd = open(PHOTO_FAVORITES_TMP,
              O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    for(index = 0; index < entry_count; ++index) {
        if(!entries[index].favorite)
            continue;
        complete =
            write_exact(fd, entries[index].path,
                        strlen(entries[index].path)) &&
            write_exact(fd, "\n", 1);
        if(!complete)
            break;
    }
    if(complete)
        complete = fsync(fd) >= 0;
    close(fd);
    if(!complete) {
        remove(PHOTO_FAVORITES_TMP);
        return false;
    }
    if(rename(PHOTO_FAVORITES_TMP, PHOTO_FAVORITES_PATH) < 0) {
        remove(PHOTO_FAVORITES_TMP);
        return false;
    }
    return true;
}

void crazypod_photo_catalog_init(void)
{
    mkdir(PHOTO_DIRECTORY);
    mkdir(PHOTO_STATE_DIRECTORY);
}

void crazypod_photo_catalog_refresh(void)
{
    entry_count = 0;
    favorites = 0;
    memset(entries, 0, sizeof(entries));
    scan_directory(PHOTO_DIRECTORY, 0);
    load_favorites();
}

int crazypod_photo_catalog_count(void)
{
    return entry_count;
}

int crazypod_photo_catalog_favorite_count(void)
{
    return favorites;
}

int crazypod_photo_catalog_favorite_index(int favorite_index)
{
    int index;

    if(favorite_index < 0)
        return -1;
    for(index = 0; index < entry_count; ++index) {
        if(entries[index].favorite && favorite_index-- == 0)
            return index;
    }
    return -1;
}

const struct crazypod_photo_catalog_entry *
crazypod_photo_catalog_get(int index)
{
    return index >= 0 && index < entry_count ? &entries[index] : NULL;
}

const char *crazypod_photo_catalog_name(int index)
{
    const struct crazypod_photo_catalog_entry *entry =
        crazypod_photo_catalog_get(index);
    const char *slash;

    if(entry == NULL)
        return "";
    slash = strrchr(entry->path, '/');
    return slash != NULL ? slash + 1 : entry->path;
}

bool crazypod_photo_catalog_toggle_favorite(int index)
{
    struct crazypod_photo_catalog_entry *entry;
    bool previous;

    if(index < 0 || index >= entry_count)
        return false;
    entry = &entries[index];
    previous = entry->favorite;
    entry->favorite = !previous;
    favorites += previous ? -1 : 1;
    if(!save_favorites()) {
        entry->favorite = previous;
        favorites += previous ? 1 : -1;
        return false;
    }
    return true;
}

#endif
