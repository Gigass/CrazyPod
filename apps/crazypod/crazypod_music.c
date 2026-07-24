#include "config.h"

#ifdef IPOD_6G

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dir.h"
#include "file.h"
#include "fs_attr.h"
#include "kernel.h"
#include "metadata.h"
#include "system.h"

#include "crazypod_music.h"
#include "crazypod_playlist.h"

#define CRAZYPOD_SCAN_DEPTH 16
#define CRAZYPOD_PLAYLIST_PATHS 64

static struct crazypod_track tracks[CRAZYPOD_MAX_TRACKS];
static struct crazypod_album albums[CRAZYPOD_MAX_TRACKS];
static char artists[CRAZYPOD_MAX_TRACKS][72];
static struct crazypod_playlist playlists[CRAZYPOD_MAX_PLAYLISTS];
static uint16_t playlist_track_indices[CRAZYPOD_MAX_PLAYLIST_TRACKS];
static char playlist_paths[CRAZYPOD_PLAYLIST_PATHS][MAX_PATH];
static const char *queue_build_paths[CRAZYPOD_QUEUE_CAPACITY];
static int track_count;
static int artist_count;
static int album_count;
static int playlist_count;
static int playlist_track_count;
static int playlist_path_count;
static volatile bool scanning;
static volatile bool scan_abort_requested;
static volatile unsigned scan_generation;
static long scan_stack[(DEFAULT_STACK_SIZE + 0x3000) / sizeof(long)];

static int compare_text(const char *left, const char *right)
{
    unsigned char a;
    unsigned char b;

    while(*left != '\0' && *right != '\0') {
        a = (unsigned char)tolower((unsigned char)*left++);
        b = (unsigned char)tolower((unsigned char)*right++);
        if(a != b)
            return a < b ? -1 : 1;
    }

    if(*left == *right)
        return 0;
    return *left == '\0' ? -1 : 1;
}

static bool text_contains(const char *text, const char *query)
{
    size_t query_length;
    const char *cursor;

    if(text == NULL || query == NULL)
        return false;
    query_length = strlen(query);
    if(query_length == 0)
        return true;
    for(cursor = text; *cursor != '\0'; ++cursor) {
        size_t i;
        for(i = 0; i < query_length; ++i) {
            unsigned char left = (unsigned char)cursor[i];
            unsigned char right = (unsigned char)query[i];
            if(left == '\0' ||
               tolower(left) != tolower(right))
                break;
        }
        if(i == query_length)
            return true;
    }
    return false;
}

static bool track_matches(const struct crazypod_track *track,
                          const char *query)
{
    return track != NULL &&
           (text_contains(track->title, query) ||
            text_contains(track->artist, query) ||
            text_contains(track->album, query));
}

static void copy_text(char *destination, size_t size, const char *source,
                      const char *fallback)
{
    if(source == NULL || source[0] == '\0')
        source = fallback;
    snprintf(destination, size, "%s", source != NULL ? source : "");
}

static const char *path_basename_local(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

static void title_from_path(char *title, size_t size, const char *path)
{
    const char *name = path_basename_local(path);
    const char *dot = strrchr(name, '.');
    size_t length = dot != NULL ? (size_t)(dot - name) : strlen(name);

    if(length >= size)
        length = size - 1;
    memcpy(title, name, length);
    title[length] = '\0';
}

static const char *extension(const char *path)
{
    const char *dot = strrchr(path_basename_local(path), '.');
    return dot != NULL ? dot + 1 : "";
}

static bool is_playlist_file(const char *path)
{
    const char *ext = extension(path);
    return compare_text(ext, "m3u") == 0 || compare_text(ext, "m3u8") == 0;
}

static bool should_skip_directory(const char *name)
{
    if(name[0] == '.')
        return true;
    return compare_text(name, "System Volume Information") == 0 ||
           compare_text(name, "RECYCLER") == 0;
}

static bool append_path(char *output, size_t size, const char *directory,
                        const char *name)
{
    int written;

    if(strcmp(directory, "/") == 0)
        written = snprintf(output, size, "/%s", name);
    else
        written = snprintf(output, size, "%s/%s", directory, name);
    return written > 0 && (size_t)written < size;
}

static int compare_tracks(const void *left_ptr, const void *right_ptr)
{
    const struct crazypod_track *left = left_ptr;
    const struct crazypod_track *right = right_ptr;
    int result = compare_text(left->title, right->title);

    if(result == 0)
        result = compare_text(left->artist, right->artist);
    if(result == 0)
        result = compare_text(left->album, right->album);
    if(result == 0)
        result = compare_text(left->path, right->path);
    return result;
}

static int compare_artist_names(const void *left, const void *right)
{
    return compare_text((const char *)left, (const char *)right);
}

static int compare_albums(const void *left_ptr, const void *right_ptr)
{
    const struct crazypod_album *left = left_ptr;
    const struct crazypod_album *right = right_ptr;
    int result = compare_text(left->title, right->title);

    if(result == 0)
        result = compare_text(left->artist, right->artist);
    return result;
}

static void add_track(const char *path, off_t source_size,
                      time_t source_mtime)
{
    struct mp3entry metadata;
    struct crazypod_track *track;
    int fd;

    if(track_count >= CRAZYPOD_MAX_TRACKS)
        return;
    if(probe_file_format(path) == AFMT_UNKNOWN)
        return;

    fd = open(path, O_RDONLY);
    if(fd < 0)
        return;

    memset(&metadata, 0, sizeof(metadata));
    if(!get_metadata(&metadata, fd, path)) {
        close(fd);
        return;
    }
    track = &tracks[track_count++];
    memset(track, 0, sizeof(*track));
    copy_text(track->path, sizeof(track->path), path, "");
    if(metadata.title != NULL && metadata.title[0] != '\0')
        copy_text(track->title, sizeof(track->title), metadata.title, "");
    else
        title_from_path(track->title, sizeof(track->title), path);
    copy_text(track->artist, sizeof(track->artist), metadata.artist,
              "Unknown Artist");
    copy_text(track->album, sizeof(track->album), metadata.album,
              "Unknown Album");
    copy_text(track->album_artist, sizeof(track->album_artist),
              metadata.albumartist,
              track->artist[0] != '\0' ? track->artist : "Unknown Artist");
    track->duration_ms = metadata.length;
    track->source_size = source_size > 0
        ? (uint32_t)source_size : 0;
    track->source_mtime = source_mtime > 0
        ? (uint32_t)source_mtime : 0;
    track->year = metadata.year > 0 ? metadata.year : 0;
    track->track_number = metadata.tracknum > 0 ? metadata.tracknum : 0;
    track->disc_number = metadata.discnum > 0 ? metadata.discnum : 0;
    track->format = metadata.codectype < 256 ? metadata.codectype : 0;
    if(metadata.has_embedded_albumart) {
        track->artwork_embedded = true;
        track->artwork_offset = metadata.albumart.pos;
        track->artwork_size = metadata.albumart.size;
        track->artwork_type =
            metadata.albumart.type & AA_CLEAR_FLAGS_MASK;
    }
    close(fd);
}

static void remember_playlist(const char *path)
{
    if(playlist_path_count >= CRAZYPOD_PLAYLIST_PATHS)
        return;
    copy_text(playlist_paths[playlist_path_count], MAX_PATH, path, "");
    ++playlist_path_count;
}

static void scan_directory(const char *path, int depth)
{
    DIR *directory;
    struct DIRENT *entry;

    if(depth > CRAZYPOD_SCAN_DEPTH)
        return;

    directory = opendir(path);
    if(directory == NULL)
        return;

    while(!scan_abort_requested &&
          (entry = readdir(directory)) != NULL) {
        struct dirinfo info;
        char child[MAX_PATH];

        if(strcmp(entry->d_name, ".") == 0 ||
           strcmp(entry->d_name, "..") == 0)
            continue;
        if(entry->d_name[0] == '.')
            continue;
        if(!append_path(child, sizeof(child), path, entry->d_name))
            continue;

        info = dir_get_info(directory, entry);
        if(info.attribute & ATTR_DIRECTORY) {
            if(!should_skip_directory(entry->d_name))
                scan_directory(child, depth + 1);
        }
        else if(is_playlist_file(child)) {
            remember_playlist(child);
        }
        else {
            add_track(child, info.size, info.mtime);
        }

        if(scan_abort_requested)
            break;
        if((track_count & 15) == 0)
            yield();
    }

    closedir(directory);
}

static bool artist_exists(const char *name)
{
    int i;
    for(i = 0; i < artist_count; ++i) {
        if(compare_text(artists[i], name) == 0)
            return true;
    }
    return false;
}

static bool album_exists(const char *title, const char *artist)
{
    int i;
    for(i = 0; i < album_count; ++i) {
        if(compare_text(albums[i].title, title) == 0 &&
           compare_text(albums[i].artist, artist) == 0)
            return true;
    }
    return false;
}

static void build_groups(void)
{
    int i;

    artist_count = 0;
    album_count = 0;
    for(i = 0; i < track_count; ++i) {
        if(!artist_exists(tracks[i].artist)) {
            copy_text(artists[artist_count], sizeof(artists[artist_count]),
                      tracks[i].artist, "Unknown Artist");
            ++artist_count;
        }
        if(!album_exists(tracks[i].album, tracks[i].album_artist)) {
            copy_text(albums[album_count].title,
                      sizeof(albums[album_count].title),
                      tracks[i].album, "Unknown Album");
            copy_text(albums[album_count].artist,
                      sizeof(albums[album_count].artist),
                      tracks[i].album_artist, "Unknown Artist");
            ++album_count;
        }
    }

    qsort(artists, artist_count, sizeof(artists[0]), compare_artist_names);
    qsort(albums, album_count, sizeof(albums[0]), compare_albums);
}

static int find_track_by_path(const char *path)
{
    int i;
    for(i = 0; i < track_count; ++i) {
        if(compare_text(tracks[i].path, path) == 0)
            return i;
    }
    return -1;
}

static void normalize_playlist_path(char *output, size_t size,
                                    const char *playlist_path,
                                    const char *entry)
{
    char combined[MAX_PATH];
    int component_starts[64];
    int component_count = 0;
    size_t read_index = 0;
    size_t write_index = 1;
    const char *slash;
    size_t directory_length;
    size_t i;

    while(*entry == ' ' || *entry == '\t')
        ++entry;

    if(entry[0] == '/' ||
       (isalpha((unsigned char)entry[0]) && entry[1] == ':')) {
        const char *absolute = entry[0] == '/' ? entry : entry + 2;
        size_t absolute_length;
        if(absolute[0] == '/')
            ++absolute;
        absolute_length = strlen(absolute);
        if(absolute_length > sizeof(combined) - 2)
            absolute_length = sizeof(combined) - 2;
        combined[0] = '/';
        memcpy(combined + 1, absolute, absolute_length);
        combined[absolute_length + 1] = '\0';
    }
    else
    {
        size_t entry_length;
        slash = strrchr(playlist_path, '/');
        directory_length = slash != NULL ? (size_t)(slash - playlist_path) : 0;
        if(directory_length >= sizeof(combined) - 1)
            directory_length = sizeof(combined) - 2;
        if(directory_length > 0)
            memcpy(combined, playlist_path, directory_length);
        else
            combined[0] = '/';
        if(directory_length == 0)
            directory_length = 1;
        combined[directory_length++] = '/';
        entry_length = strlen(entry);
        if(entry_length > sizeof(combined) - directory_length - 1)
            entry_length = sizeof(combined) - directory_length - 1;
        memcpy(combined + directory_length, entry, entry_length);
        combined[directory_length + entry_length] = '\0';
    }

    for(i = 0; combined[i] != '\0'; ++i) {
        if(combined[i] == '\\')
            combined[i] = '/';
    }

    if(size == 0)
        return;
    output[0] = '/';
    output[1 < size ? 1 : 0] = '\0';

    while(combined[read_index] != '\0') {
        size_t start;
        size_t length;

        while(combined[read_index] == '/')
            ++read_index;
        if(combined[read_index] == '\0')
            break;
        start = read_index;
        while(combined[read_index] != '\0' &&
              combined[read_index] != '/')
            ++read_index;
        length = read_index - start;

        if(length == 1 && combined[start] == '.')
            continue;
        if(length == 2 && combined[start] == '.' &&
           combined[start + 1] == '.') {
            if(component_count > 0) {
                write_index = (size_t)component_starts[--component_count];
                output[write_index] = '\0';
            }
            continue;
        }
        if(component_count >= (int)(sizeof(component_starts) /
                                    sizeof(component_starts[0])))
            break;
        if(write_index > 1 && write_index + 1 < size)
            output[write_index++] = '/';
        component_starts[component_count++] = (int)(write_index > 1
            ? write_index - 1 : write_index);
        if(write_index + length >= size)
            length = size - write_index - 1;
        memcpy(output + write_index, combined + start, length);
        write_index += length;
        output[write_index] = '\0';
    }
}

static int read_playlist_line(int fd, char *line, size_t size)
{
    size_t length = 0;
    char character;
    int result;

    while((result = read(fd, &character, 1)) == 1) {
        if(character == '\n')
            break;
        if(character == '\r')
            continue;
        if(length + 1 < size)
            line[length++] = character;
    }
    line[length] = '\0';
    return result == 1 || length > 0 ? (int)length : -1;
}

static void parse_playlists(void)
{
    int path_index;

    playlist_count = 0;
    playlist_track_count = 0;
    for(path_index = 0;
        path_index < playlist_path_count &&
        playlist_count < CRAZYPOD_MAX_PLAYLISTS;
        ++path_index) {
        struct crazypod_playlist *playlist = &playlists[playlist_count];
        char line[MAX_PATH];
        int fd = open(playlist_paths[path_index], O_RDONLY);

        if(fd < 0)
            continue;

        memset(playlist, 0, sizeof(*playlist));
        title_from_path(playlist->name, sizeof(playlist->name),
                        playlist_paths[path_index]);
        playlist->first_track = playlist_track_count;

        while(read_playlist_line(fd, line, sizeof(line)) >= 0) {
            char resolved[MAX_PATH];
            int track_index;
            size_t length;

            if((unsigned char)line[0] == 0xef &&
               (unsigned char)line[1] == 0xbb &&
               (unsigned char)line[2] == 0xbf)
                memmove(line, line + 3, strlen(line + 3) + 1);
            if(line[0] == '\0' || line[0] == '#')
                continue;
            length = strlen(line);
            while(length > 0 &&
                  (line[length - 1] == ' ' || line[length - 1] == '\t'))
                line[--length] = '\0';

            normalize_playlist_path(resolved, sizeof(resolved),
                                    playlist_paths[path_index], line);
            track_index = find_track_by_path(resolved);
            if(track_index >= 0 &&
               playlist_track_count < CRAZYPOD_MAX_PLAYLIST_TRACKS) {
                playlist_track_indices[playlist_track_count++] = track_index;
                ++playlist->track_count;
            }
        }

        close(fd);
        if(playlist->track_count > 0)
            ++playlist_count;
    }
}

static int compare_track_order(const struct crazypod_track *left,
                               const struct crazypod_track *right)
{
    if(left->disc_number != right->disc_number)
        return left->disc_number < right->disc_number ? -1 : 1;
    if(left->track_number != right->track_number)
        return left->track_number < right->track_number ? -1 : 1;
    {
        int result = compare_text(left->title, right->title);
        return result != 0 ? result : compare_text(left->path, right->path);
    }
}

static void sort_queue_paths_by_track_order(int count)
{
    int i;
    for(i = 1; i < count; ++i) {
        const char *path = queue_build_paths[i];
        int track_index = find_track_by_path(path);
        int j = i;

        while(j > 0) {
            int previous_index = find_track_by_path(queue_build_paths[j - 1]);
            if(track_index < 0 || previous_index < 0 ||
               compare_track_order(&tracks[previous_index],
                                   &tracks[track_index]) <= 0)
                break;
            queue_build_paths[j] = queue_build_paths[j - 1];
            --j;
        }
        queue_build_paths[j] = path;
    }
}

void crazypod_music_init(void)
{
    track_count = 0;
    artist_count = 0;
    album_count = 0;
    playlist_count = 0;
    playlist_track_count = 0;
    playlist_path_count = 0;
    scanning = false;
    scan_abort_requested = false;
    scan_generation = 0;
}

void crazypod_music_scan(void)
{
    scanning = true;
    track_count = 0;
    playlist_path_count = 0;
    scan_directory("/Music", 0);
    if(!scan_abort_requested)
        scan_directory("/iPod_Control/Music", 0);
    if(!scan_abort_requested) {
        qsort(tracks, track_count, sizeof(tracks[0]), compare_tracks);
        build_groups();
        parse_playlists();
    }
    else {
        track_count = 0;
        artist_count = 0;
        album_count = 0;
        playlist_count = 0;
        playlist_track_count = 0;
        playlist_path_count = 0;
    }
    scanning = false;
    ++scan_generation;
}

static void scan_thread(void)
{
    crazypod_music_scan();
}

bool crazypod_music_scan_async(void)
{
    unsigned int id;

    if(scanning)
        return false;
    scan_abort_requested = false;
    scanning = true;
    id = create_thread(scan_thread, scan_stack, sizeof(scan_stack), 0,
                       "crazypod scan"
                       IF_PRIO(, PRIORITY_BACKGROUND)
                       IF_COP(, CPU));
    if(id == 0) {
        scanning = false;
        return false;
    }
    return true;
}

void crazypod_music_cancel_scan(void)
{
    if(!scanning)
        return;

    scan_abort_requested = true;
    while(scanning)
        yield();
}

bool crazypod_music_is_scanning(void)
{
    return scanning;
}

unsigned crazypod_music_scan_generation(void)
{
    return scan_generation;
}

int crazypod_music_track_count(void)
{
    return track_count;
}

const struct crazypod_track *crazypod_music_track(int index)
{
    return index >= 0 && index < track_count ? &tracks[index] : NULL;
}

int crazypod_music_find_track(const char *path)
{
    return path != NULL ? find_track_by_path(path) : -1;
}

int crazypod_music_artist_count(void)
{
    return artist_count;
}

const char *crazypod_music_artist(int index)
{
    return index >= 0 && index < artist_count ? artists[index] : NULL;
}

int crazypod_music_artist_track_count(int artist_index)
{
    const char *artist = crazypod_music_artist(artist_index);
    int count = 0;
    int i;

    if(artist == NULL)
        return 0;
    for(i = 0; i < track_count; ++i) {
        if(compare_text(tracks[i].artist, artist) == 0)
            ++count;
    }
    return count;
}

const struct crazypod_track *crazypod_music_artist_track(int artist_index,
                                                          int track_index)
{
    const char *artist = crazypod_music_artist(artist_index);
    int visible = 0;
    int i;

    if(artist == NULL || track_index < 0)
        return NULL;
    for(i = 0; i < track_count; ++i) {
        if(compare_text(tracks[i].artist, artist) == 0) {
            if(visible == track_index)
                return &tracks[i];
            ++visible;
        }
    }
    return NULL;
}

int crazypod_music_album_count(void)
{
    return album_count;
}

const struct crazypod_album *crazypod_music_album(int index)
{
    return index >= 0 && index < album_count ? &albums[index] : NULL;
}

int crazypod_music_album_track_count(int album_index)
{
    const struct crazypod_album *album = crazypod_music_album(album_index);
    int count = 0;
    int i;

    if(album == NULL)
        return 0;
    for(i = 0; i < track_count; ++i) {
        if(compare_text(tracks[i].album, album->title) == 0 &&
           compare_text(tracks[i].album_artist, album->artist) == 0)
            ++count;
    }
    return count;
}

const struct crazypod_track *crazypod_music_album_track(int album_index,
                                                         int track_index)
{
    const struct crazypod_album *album = crazypod_music_album(album_index);
    const struct crazypod_track *best = NULL;
    int visible;
    int i;

    if(album == NULL || track_index < 0)
        return NULL;

    /*
     * Album lists are small. Select the Nth track in disc/track order without
     * allocating a second permanent index table.
     */
    for(visible = 0; visible <= track_index; ++visible) {
        best = NULL;
        for(i = 0; i < track_count; ++i) {
            const struct crazypod_track *candidate = &tracks[i];
            int before = 0;
            int j;

            if(compare_text(candidate->album, album->title) != 0 ||
               compare_text(candidate->album_artist, album->artist) != 0)
                continue;
            for(j = 0; j < track_count; ++j) {
                const struct crazypod_track *other = &tracks[j];
                if(compare_text(other->album, album->title) == 0 &&
                   compare_text(other->album_artist, album->artist) == 0 &&
                   compare_track_order(other, candidate) < 0)
                    ++before;
            }
            if(before == visible) {
                best = candidate;
                break;
            }
        }
        if(best == NULL)
            return NULL;
    }
    return best;
}

int crazypod_music_playlist_count(void)
{
    return playlist_count;
}

const struct crazypod_playlist *crazypod_music_playlist(int index)
{
    return index >= 0 && index < playlist_count ? &playlists[index] : NULL;
}

const struct crazypod_track *crazypod_music_playlist_track(int playlist_index,
                                                            int track_index)
{
    const struct crazypod_playlist *playlist =
        crazypod_music_playlist(playlist_index);
    int pool_index;

    if(playlist == NULL || track_index < 0 ||
       track_index >= playlist->track_count)
        return NULL;
    pool_index = playlist->first_track + track_index;
    return crazypod_music_track(playlist_track_indices[pool_index]);
}

int crazypod_music_search_count(const char *query)
{
    int count = 0;
    int i;

    for(i = 0; i < track_count; ++i) {
        if(track_matches(&tracks[i], query))
            ++count;
    }
    return count;
}

const struct crazypod_track *crazypod_music_search_track(const char *query,
                                                          int result_index)
{
    int visible = 0;
    int i;

    if(result_index < 0)
        return NULL;
    for(i = 0; i < track_count; ++i) {
        if(track_matches(&tracks[i], query)) {
            if(visible == result_index)
                return &tracks[i];
            ++visible;
        }
    }
    return NULL;
}

bool crazypod_music_play_track(int library_index)
{
    const struct crazypod_track *track = crazypod_music_track(library_index);
    if(track == NULL)
        return false;
    queue_build_paths[0] = track->path;
    crazypod_queue_replace(queue_build_paths, 1, 0);
    return true;
}

bool crazypod_music_play_search(const char *query, int selected_index)
{
    int count = 0;
    int start = -1;
    int visible = 0;
    int i;

    for(i = 0; i < track_count && count < CRAZYPOD_QUEUE_CAPACITY; ++i) {
        if(!track_matches(&tracks[i], query))
            continue;
        if(visible == selected_index)
            start = count;
        queue_build_paths[count++] = tracks[i].path;
        ++visible;
    }
    if(count <= 0)
        return false;
    if(start < 0)
        start = 0;
    crazypod_queue_replace(queue_build_paths, count, start);
    return true;
}

bool crazypod_music_play(enum crazypod_music_scope scope, int group_index,
                         int selected_index)
{
    const char *selected_path = NULL;
    int count = 0;
    int start = -1;
    int i;

    if(scope == CRAZYPOD_SCOPE_PLAYLIST) {
        const struct crazypod_playlist *playlist =
            crazypod_music_playlist(group_index);
        if(playlist == NULL)
            return false;
        for(i = 0; i < playlist->track_count &&
                   count < CRAZYPOD_QUEUE_CAPACITY; ++i) {
            const struct crazypod_track *track =
                crazypod_music_playlist_track(group_index, i);
            if(track != NULL) {
                if(i == selected_index)
                    start = count;
                queue_build_paths[count++] = track->path;
            }
        }
    }
    else if(scope == CRAZYPOD_SCOPE_ALBUM) {
        int album_tracks = crazypod_music_album_track_count(group_index);
        for(i = 0; i < album_tracks &&
                   count < CRAZYPOD_QUEUE_CAPACITY; ++i) {
            const struct crazypod_track *track =
                crazypod_music_album_track(group_index, i);
            if(track != NULL) {
                if(i == selected_index)
                    selected_path = track->path;
                queue_build_paths[count++] = track->path;
            }
        }
        sort_queue_paths_by_track_order(count);
        if(selected_path != NULL) {
            for(i = 0; i < count; ++i) {
                if(strcmp(queue_build_paths[i], selected_path) == 0) {
                    start = i;
                    break;
                }
            }
        }
    }
    else {
        const char *artist = scope == CRAZYPOD_SCOPE_ARTIST
            ? crazypod_music_artist(group_index) : NULL;
        int visible_index = 0;

        for(i = 0; i < track_count && count < CRAZYPOD_QUEUE_CAPACITY; ++i) {
            bool include = scope == CRAZYPOD_SCOPE_ALL;
            if(scope == CRAZYPOD_SCOPE_ARTIST && artist != NULL)
                include = compare_text(tracks[i].artist, artist) == 0;

            if(include) {
                if(visible_index == selected_index)
                    start = count;
                queue_build_paths[count++] = tracks[i].path;
                ++visible_index;
            }
        }
    }

    if(count <= 0)
        return false;
    if(start < 0)
        start = 0;
    crazypod_queue_replace(queue_build_paths, count, start);
    return true;
}

#endif
