#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dir.h"
#include "file.h"
#include "kernel.h"
#include "pathfuncs.h"

#include "../crazypod_videos.h"
#include "crazypod_video_catalog.h"
#include "crazypod_video_engine.h"

#define VIDEO_DIRECTORY "/Videos"
#define VIDEO_DIRECTORY_DEPTH 4
#define VIDEO_STATE_DIRECTORY "/.crazypod"
#define VIDEO_STATE_PATH VIDEO_STATE_DIRECTORY "/video-resume.bin"
#define VIDEO_STATE_TMP VIDEO_STATE_DIRECTORY "/video-resume.tmp"
#define VIDEO_STATE_MAGIC 0x43505652u
#define VIDEO_STATE_VERSION 1u
#define VIDEO_CACHE_DIRECTORY VIDEO_STATE_DIRECTORY "/cache"
#define VIDEO_CATALOG_PATH VIDEO_CACHE_DIRECTORY "/video-catalog.bin"
#define VIDEO_CATALOG_TMP VIDEO_CACHE_DIRECTORY "/video-catalog.tmp"
#define MEDIA_INVALID_PATH VIDEO_CACHE_DIRECTORY "/media.invalid"
#define VIDEO_CATALOG_MAGIC 0x43505643u
#define VIDEO_CATALOG_VERSION 1u

struct video_resume_file_header {
    uint32_t magic;
    uint16_t version;
    uint16_t entry_size;
    uint32_t count;
};

struct video_resume_disk_entry {
    char path[MAX_PATH];
    uint32_t size;
    uint32_t mtime;
    uint32_t resume_ticks;
    uint32_t duration_ticks;
};

struct video_catalog_header {
    uint32_t magic;
    uint32_t version;
    uint32_t entry_size;
    uint32_t count;
    uint32_t checksum;
};

static struct crazypod_video_catalog_entry
    entries[CRAZYPOD_VIDEO_MAX_FILES];
static struct crazypod_video_catalog_entry
    scan_entries[CRAZYPOD_VIDEO_MAX_FILES];
static int entry_count;
static struct mutex catalog_mutex;
static struct mutex catalog_io_mutex;
static volatile bool refresh_abort_requested;

static uint32_t checksum_update(uint32_t hash, const void *data, size_t size)
{
    const uint8_t *bytes = data;

    while(size-- > 0) {
        hash ^= *bytes++;
        hash *= 16777619u;
    }
    return hash;
}

static bool write_exact(int fd, const void *data, size_t size)
{
    const uint8_t *cursor = data;

    while(size > 0) {
        ssize_t written = write(fd, cursor, size);
        if(written <= 0)
            return false;
        cursor += written;
        size -= (size_t)written;
    }
    return true;
}

static bool read_exact(int fd, void *data, size_t size)
{
    uint8_t *cursor = data;

    while(size > 0) {
        ssize_t count = read(fd, cursor, size);
        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

bool crazypod_video_catalog_path_supported(const char *path)
{
    return crazypod_video_engine_path_supported(path);
}

static int compare_entries(
    const struct crazypod_video_catalog_entry *left,
    const struct crazypod_video_catalog_entry *right)
{
    int result = strcasecmp(left->name, right->name);

    return result != 0 ? result : strcasecmp(left->path, right->path);
}

static void name_from_path(char *name, size_t size, const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *base = slash != NULL ? slash + 1 : path;
    char *extension;

    snprintf(name, size, "%s", base);
    extension = strrchr(name, '.');
    if(extension != NULL)
        *extension = '\0';
}

static void poster_path(char *poster, size_t size, const char *path)
{
    char *extension;

    snprintf(poster, size, "%s", path);
    extension = strrchr(poster, '.');
    if(extension != NULL)
        snprintf(
            extension, size - (size_t)(extension - poster), ".bmp");
}

static void disk_entry_from_catalog(
    struct video_resume_disk_entry *disk_entry,
    const struct crazypod_video_catalog_entry *entry)
{
    memset(disk_entry, 0, sizeof(*disk_entry));
    memcpy(disk_entry->path, entry->path,
           sizeof(disk_entry->path));
    disk_entry->path[sizeof(disk_entry->path) - 1] = '\0';
    disk_entry->size = entry->size;
    disk_entry->mtime = entry->mtime;
    disk_entry->resume_ticks = entry->resume_ticks;
    disk_entry->duration_ticks = entry->duration_ticks;
}

static void insert_entry(
    const char *path, const struct dirinfo *info,
    struct crazypod_video_catalog_entry *catalog, int *count)
{
    struct crazypod_video_catalog_entry entry;
    int position;

    if(*count >= CRAZYPOD_VIDEO_MAX_FILES)
        return;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.path, sizeof(entry.path), "%s", path);
    poster_path(entry.poster_path, sizeof(entry.poster_path), path);
    name_from_path(entry.name, sizeof(entry.name), path);
    entry.size = info->size;
    entry.mtime = info->mtime;
    position = *count;
    while(position > 0 &&
          compare_entries(&entry, &catalog[position - 1]) < 0) {
        catalog[position] = catalog[position - 1];
        --position;
    }
    catalog[position] = entry;
    ++(*count);
}

static void scan_directory(
    const char *path, int depth,
    struct crazypod_video_catalog_entry *catalog, int *count)
{
    DIR *directory;
    struct dirent *entry;

    if(refresh_abort_requested ||
       depth > VIDEO_DIRECTORY_DEPTH ||
       *count >= CRAZYPOD_VIDEO_MAX_FILES)
        return;
    directory = opendir(path);
    if(directory == NULL) {
#ifdef SIMULATOR
        if(getenv("CRAZYPOD_SIM_VIDEO_DIAGNOSTICS") != NULL)
            fprintf(stderr, "CrazyPod video catalog: cannot open %s\n", path);
#endif
        return;
    }
    while(!refresh_abort_requested &&
          *count < CRAZYPOD_VIDEO_MAX_FILES &&
          (entry = readdir(directory)) != NULL) {
        struct dirinfo info = dir_get_info(directory, entry);
        char child[MAX_PATH];

        if(entry->d_name[0] == '.')
            continue;
        if(path_append(child, path, entry->d_name, sizeof(child)) >=
           (int)sizeof(child))
            continue;
        if(info.attribute & ATTR_DIRECTORY)
            scan_directory(child, depth + 1, catalog, count);
        else if(crazypod_video_catalog_path_supported(child)) {
#ifdef SIMULATOR
            if(getenv("CRAZYPOD_SIM_VIDEO_DIAGNOSTICS") != NULL)
                fprintf(stderr, "CrazyPod video catalog: found %s\n", child);
#endif
            insert_entry(child, &info, catalog, count);
        }
        yield();
    }
    closedir(directory);
}

static int index_for_path(
    const struct crazypod_video_catalog_entry *catalog, int count,
    const char *path, uint32_t size, uint32_t mtime)
{
    int index;

    for(index = 0; index < count; ++index) {
        if(catalog[index].size == size &&
           catalog[index].mtime == mtime &&
           strcmp(catalog[index].path, path) == 0)
            return index;
    }
    return -1;
}

static void load_state(
    struct crazypod_video_catalog_entry *catalog, int count)
{
    struct video_resume_file_header header;
    int fd = open(VIDEO_STATE_PATH, O_RDONLY);
    uint32_t record;

    if(fd < 0)
        return;
    if(!read_exact(fd, &header, sizeof(header)) ||
       header.magic != VIDEO_STATE_MAGIC ||
       header.version != VIDEO_STATE_VERSION ||
       header.entry_size != sizeof(struct video_resume_disk_entry) ||
       header.count > CRAZYPOD_VIDEO_MAX_FILES) {
        close(fd);
        return;
    }
    for(record = 0; record < header.count; ++record) {
        struct video_resume_disk_entry disk_entry;
        int index;

        if(!read_exact(fd, &disk_entry, sizeof(disk_entry)))
            break;
        if(memchr(disk_entry.path, '\0',
                  sizeof(disk_entry.path)) == NULL)
            continue;
        index = index_for_path(
            catalog, count, disk_entry.path,
            disk_entry.size, disk_entry.mtime);
        if(index >= 0) {
            catalog[index].resume_ticks = disk_entry.resume_ticks;
            catalog[index].duration_ticks = disk_entry.duration_ticks;
        }
    }
    close(fd);
}

static bool save_state(
    const struct crazypod_video_catalog_entry *catalog, int count)
{
    struct video_resume_file_header header;
    int fd;
    int index;
    bool complete;

    mkdir(VIDEO_STATE_DIRECTORY);
    fd = open(VIDEO_STATE_TMP, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    header.magic = VIDEO_STATE_MAGIC;
    header.version = VIDEO_STATE_VERSION;
    header.entry_size = sizeof(struct video_resume_disk_entry);
    header.count = (uint32_t)count;
    complete = write_exact(fd, &header, sizeof(header));
    for(index = 0; complete && index < count; ++index) {
        struct video_resume_disk_entry disk_entry;

        memset(&disk_entry, 0, sizeof(disk_entry));
        memcpy(disk_entry.path, catalog[index].path,
               sizeof(disk_entry.path));
        disk_entry.path[sizeof(disk_entry.path) - 1] = '\0';
        disk_entry.size = catalog[index].size;
        disk_entry.mtime = catalog[index].mtime;
        disk_entry.resume_ticks = catalog[index].resume_ticks;
        disk_entry.duration_ticks = catalog[index].duration_ticks;
        complete = write_exact(fd, &disk_entry, sizeof(disk_entry));
    }
    if(complete)
        complete = fsync(fd) >= 0;
    close(fd);
    if(!complete) {
        remove(VIDEO_STATE_TMP);
        return false;
    }
    if(rename(VIDEO_STATE_TMP, VIDEO_STATE_PATH) < 0) {
        remove(VIDEO_STATE_TMP);
        return false;
    }
    return true;
}

static uint32_t catalog_checksum(
    const struct video_catalog_header *source,
    const struct crazypod_video_catalog_entry *catalog)
{
    struct video_catalog_header header = *source;
    struct video_resume_disk_entry disk_entry;
    uint32_t hash = 2166136261u;
    int index;

    header.checksum = 0;
    hash = checksum_update(hash, &header, sizeof(header));
    for(index = 0; index < (int)header.count; ++index) {
        disk_entry_from_catalog(&disk_entry, &catalog[index]);
        hash = checksum_update(
            hash, &disk_entry, sizeof(disk_entry));
    }
    return hash;
}

static bool load_catalog(void)
{
    struct video_catalog_header header;
    uint32_t hash;
    uint32_t record;
    int marker_fd = open(MEDIA_INVALID_PATH, O_RDONLY);
    int fd;

    if(marker_fd >= 0) {
        close(marker_fd);
        return false;
    }
    fd = open(VIDEO_CATALOG_PATH, O_RDONLY);
    if(fd < 0)
        return false;
    if(!read_exact(fd, &header, sizeof(header)) ||
       header.magic != VIDEO_CATALOG_MAGIC ||
       header.version != VIDEO_CATALOG_VERSION ||
       header.entry_size != sizeof(struct video_resume_disk_entry) ||
       header.count > CRAZYPOD_VIDEO_MAX_FILES) {
        close(fd);
        return false;
    }
    {
        struct video_catalog_header checksum_header = header;

        checksum_header.checksum = 0;
        hash = checksum_update(
            2166136261u, &checksum_header,
            sizeof(checksum_header));
    }
    for(record = 0; record < header.count; ++record) {
        struct video_resume_disk_entry disk_entry;
        struct crazypod_video_catalog_entry *entry =
            &entries[record];

        if(!read_exact(fd, &disk_entry, sizeof(disk_entry))) {
            close(fd);
            return false;
        }
        hash = checksum_update(
            hash, &disk_entry, sizeof(disk_entry));
        if(memchr(disk_entry.path, '\0',
                  sizeof(disk_entry.path)) == NULL ||
           disk_entry.path[0] != '/' ||
           !crazypod_video_catalog_path_supported(
               disk_entry.path)) {
            close(fd);
            return false;
        }
        memset(entry, 0, sizeof(*entry));
        snprintf(entry->path, sizeof(entry->path), "%s",
                 disk_entry.path);
        poster_path(
            entry->poster_path, sizeof(entry->poster_path),
            entry->path);
        name_from_path(
            entry->name, sizeof(entry->name), entry->path);
        entry->size = disk_entry.size;
        entry->mtime = disk_entry.mtime;
        entry->resume_ticks = disk_entry.resume_ticks;
        entry->duration_ticks = disk_entry.duration_ticks;
    }
    close(fd);
    if(hash != header.checksum)
        return false;
    entry_count = (int)header.count;
    return true;
}

static bool save_catalog(
    const struct crazypod_video_catalog_entry *catalog, int count)
{
    struct video_catalog_header header;
    struct video_resume_disk_entry disk_entry;
    bool complete;
    int index;
    int fd;

    mkdir(VIDEO_STATE_DIRECTORY);
    mkdir(VIDEO_CACHE_DIRECTORY);
    memset(&header, 0, sizeof(header));
    header.magic = VIDEO_CATALOG_MAGIC;
    header.version = VIDEO_CATALOG_VERSION;
    header.entry_size = sizeof(disk_entry);
    header.count = (uint32_t)count;
    header.checksum = catalog_checksum(&header, catalog);
    remove(VIDEO_CATALOG_TMP);
    fd = open(VIDEO_CATALOG_TMP,
              O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    complete = write_exact(fd, &header, sizeof(header));
    for(index = 0; complete && index < count; ++index) {
        disk_entry_from_catalog(&disk_entry, &catalog[index]);
        complete = write_exact(
            fd, &disk_entry, sizeof(disk_entry));
    }
    if(complete)
        complete = fsync(fd) >= 0;
    close(fd);
    if(!complete ||
       rename(VIDEO_CATALOG_TMP, VIDEO_CATALOG_PATH) < 0) {
        remove(VIDEO_CATALOG_TMP);
        return false;
    }
    return true;
}

bool crazypod_video_catalog_init(void)
{
    mkdir(VIDEO_DIRECTORY);
    mkdir(VIDEO_STATE_DIRECTORY);
    mkdir(VIDEO_CACHE_DIRECTORY);
    entry_count = 0;
    memset(entries, 0, sizeof(entries));
    mutex_init(&catalog_mutex);
    mutex_init(&catalog_io_mutex);
    return load_catalog();
}

void crazypod_video_catalog_refresh(void)
{
    int scan_count = 0;

    mutex_lock(&catalog_io_mutex);
    if(refresh_abort_requested) {
        mutex_unlock(&catalog_io_mutex);
        return;
    }
    memset(scan_entries, 0, sizeof(scan_entries));
    scan_directory(VIDEO_DIRECTORY, 0, scan_entries, &scan_count);
    if(!refresh_abort_requested) {
        load_state(scan_entries, scan_count);
        if(!refresh_abort_requested) {
            (void)save_catalog(scan_entries, scan_count);
        }
    }
    if(!refresh_abort_requested) {
        mutex_lock(&catalog_mutex);
        memcpy(entries, scan_entries,
               (size_t)scan_count * sizeof(entries[0]));
        if(scan_count < CRAZYPOD_VIDEO_MAX_FILES)
            memset(&entries[scan_count], 0,
                   (size_t)(CRAZYPOD_VIDEO_MAX_FILES - scan_count) *
                   sizeof(entries[0]));
        entry_count = scan_count;
        mutex_unlock(&catalog_mutex);
    }
    mutex_unlock(&catalog_io_mutex);
}

void crazypod_video_catalog_cancel_refresh(void)
{
    refresh_abort_requested = true;
}

void crazypod_video_catalog_reset_refresh_cancel(void)
{
    refresh_abort_requested = false;
}

bool crazypod_video_catalog_save(void)
{
    bool saved;
    int count;

    mutex_lock(&catalog_io_mutex);
    mutex_lock(&catalog_mutex);
    count = entry_count;
    memcpy(scan_entries, entries,
           (size_t)count * sizeof(entries[0]));
    mutex_unlock(&catalog_mutex);
    saved = !refresh_abort_requested &&
        save_state(scan_entries, count) &&
        save_catalog(scan_entries, count);
    mutex_unlock(&catalog_io_mutex);
    return saved;
}

void crazypod_video_catalog_invalidate(void)
{
    refresh_abort_requested = true;
    mutex_lock(&catalog_io_mutex);
    mutex_lock(&catalog_mutex);
    remove(VIDEO_CATALOG_TMP);
    remove(VIDEO_CATALOG_PATH);
    mutex_unlock(&catalog_mutex);
    mutex_unlock(&catalog_io_mutex);
}

int crazypod_video_catalog_count(void)
{
    int count;

    mutex_lock(&catalog_mutex);
    count = entry_count;
    mutex_unlock(&catalog_mutex);
    return count;
}

const struct crazypod_video_catalog_entry *
crazypod_video_catalog_get(int index)
{
    const struct crazypod_video_catalog_entry *entry = NULL;

    mutex_lock(&catalog_mutex);
    if(index >= 0 && index < entry_count)
        entry = &entries[index];
    mutex_unlock(&catalog_mutex);
    return entry;
}

bool crazypod_video_catalog_copy(
    int index, struct crazypod_video_catalog_entry *entry)
{
    bool copied = false;

    if(entry == NULL)
        return false;
    mutex_lock(&catalog_mutex);
    if(index >= 0 && index < entry_count) {
        *entry = entries[index];
        copied = true;
    }
    mutex_unlock(&catalog_mutex);
    return copied;
}

bool crazypod_video_catalog_update_playback(
    int index, uint32_t resume_ticks, uint32_t duration_ticks)
{
    mutex_lock(&catalog_mutex);
    if(index < 0 || index >= entry_count)
    {
        mutex_unlock(&catalog_mutex);
        return false;
    }
    entries[index].resume_ticks = resume_ticks;
    entries[index].duration_ticks = duration_ticks;
    mutex_unlock(&catalog_mutex);
    return true;
}

bool crazypod_video_catalog_delete(int index)
{
    char path[MAX_PATH];
    size_t path_length;
    int entry_index;

    /* Refresh and save both use this lock before touching the catalog. Keep
     * deletion in that same order so a refresh cannot publish a just-deleted
     * file back into the UI. */
    mutex_lock(&catalog_io_mutex);
    mutex_lock(&catalog_mutex);
    if(index < 0 || index >= entry_count) {
        mutex_unlock(&catalog_mutex);
        mutex_unlock(&catalog_io_mutex);
        return false;
    }
    snprintf(path, sizeof(path), "%s", entries[index].path);
    mutex_unlock(&catalog_mutex);

    path_length = strlen(path);
    if(strncmp(path, VIDEO_DIRECTORY "/",
               sizeof(VIDEO_DIRECTORY)) != 0 ||
       strstr(path, "/../") != NULL ||
       (path_length >= 3 &&
        strcmp(path + path_length - 3, "/..") == 0) ||
       !crazypod_video_catalog_path_supported(path) ||
       remove(path) < 0) {
        mutex_unlock(&catalog_io_mutex);
        return false;
    }

    mutex_lock(&catalog_mutex);
    entry_index = -1;
    for(index = 0; index < entry_count; ++index) {
        if(strcmp(entries[index].path, path) == 0) {
            entry_index = index;
            break;
        }
    }
    if(entry_index >= 0) {
        if(entry_index + 1 < entry_count)
            memmove(&entries[entry_index], &entries[entry_index + 1],
                    (size_t)(entry_count - entry_index - 1) *
                    sizeof(entries[0]));
        --entry_count;
        memset(&entries[entry_count], 0, sizeof(entries[0]));
    }
    mutex_unlock(&catalog_mutex);
    mutex_unlock(&catalog_io_mutex);
    return true;
}

#endif
