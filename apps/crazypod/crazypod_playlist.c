#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "metadata.h"
#include "playlist.h"
#include "settings.h"

#include "crazypod_playlist.h"

static char queue_paths[CRAZYPOD_QUEUE_CAPACITY][MAX_PATH];
static char original_paths[CRAZYPOD_QUEUE_CAPACITY][MAX_PATH];
static int queue_length;
static int queue_index;
static bool queue_started;
static bool queue_shuffle;
static uint32_t shuffle_state = 0x43505f36;
static unsigned queue_generation;
static struct playlist_info queue_info;

static int normalize_index(int index, bool allow_repeat)
{
    if(queue_length <= 0)
        return -1;

    if(index >= 0 && index < queue_length)
        return index;

    if(allow_repeat && global_settings.repeat_mode == REPEAT_ALL) {
        index %= queue_length;
        if(index < 0)
            index += queue_length;
        return index;
    }

    return -1;
}

static uint32_t next_random(void)
{
    shuffle_state = shuffle_state * 1664525u + 1013904223u;
    return shuffle_state;
}

static void copy_path(char *destination, const char *source)
{
    if(source == NULL)
        source = "";
    snprintf(destination, MAX_PATH, "%s", source);
}

static bool path_is_excluded_ipod_music(const char *path)
{
    static const char directory[] = "/iPod_Control/Music";
    size_t length = sizeof(directory) - 1;

    return path != NULL &&
        strncmp(path, directory, length) == 0 &&
        (path[length] == '\0' || path[length] == '/');
}

void playlist_init(void)
{
    memset(&queue_info, 0, sizeof(queue_info));
    mutex_init(&queue_info.mutex);
    queue_info.index = 0;
    queue_info.max_playlist_size = CRAZYPOD_QUEUE_CAPACITY;
    queue_info.fd = -1;
    queue_info.control_fd = -1;
    queue_length = 0;
    queue_index = 0;
    queue_started = false;
    queue_shuffle = false;
    queue_generation = 1;
}

void playlist_shutdown(void)
{
    audio_stop();
}

int playlist_create(const char *dir, const char *file)
{
    (void)dir;
    (void)file;
    audio_stop();
    queue_length = 0;
    queue_index = 0;
    queue_started = false;
    queue_info.amount = 0;
    queue_info.index = 0;
    queue_info.started = false;
    ++queue_generation;
    return 0;
}

int playlist_insert_track(struct playlist_info *playlist, const char *filename,
                          int position, bool queued, bool sync)
{
    int insert_at;
    int i;

    (void)playlist;
    (void)queued;
    (void)sync;

    if(filename == NULL || path_is_excluded_ipod_music(filename) ||
       queue_length >= CRAZYPOD_QUEUE_CAPACITY)
        return -1;

    if(position == PLAYLIST_PREPEND || position == PLAYLIST_INSERT_FIRST)
        insert_at = 0;
    else if(position >= 0 && position <= queue_length)
        insert_at = position;
    else
        insert_at = queue_length;

    for(i = queue_length; i > insert_at; --i) {
        copy_path(queue_paths[i], queue_paths[i - 1]);
        copy_path(original_paths[i], original_paths[i - 1]);
    }

    copy_path(queue_paths[insert_at], filename);
    copy_path(original_paths[insert_at], filename);
    ++queue_length;
    queue_info.amount = queue_length;
    ++queue_generation;
    return insert_at;
}

const char *playlist_peek(int steps, char *buffer, size_t buffer_size)
{
    int index = normalize_index(queue_index + steps, true);
    const char *path;

    if(index < 0)
        return NULL;

    path = queue_paths[index];
    if(buffer != NULL && buffer_size > 0) {
        snprintf(buffer, buffer_size, "%s", path);
        return buffer;
    }

    return path;
}

bool playlist_check(int steps)
{
    return normalize_index(queue_index + steps, false) >= 0;
}

int playlist_next(int steps)
{
    int next;

    if(global_settings.repeat_mode == REPEAT_ONE && steps != 0)
        next = queue_index;
    else
        next = normalize_index(queue_index + steps, true);

    if(next < 0)
        return -1;

    queue_index = next;
    queue_info.index = next;
    return next;
}

bool playlist_next_dir(int direction)
{
    (void)direction;
    return false;
}

void playlist_skip_entry(struct playlist_info *playlist, int steps)
{
    int index = normalize_index(queue_index + steps, false);
    int i;

    (void)playlist;
    if(index < 0)
        return;

    for(i = index; i + 1 < queue_length; ++i) {
        copy_path(queue_paths[i], queue_paths[i + 1]);
        copy_path(original_paths[i], original_paths[i + 1]);
    }

    --queue_length;
    if(queue_length <= 0)
        queue_index = 0;
    else if(queue_index >= queue_length)
        queue_index = queue_length - 1;
    queue_info.amount = queue_length;
    queue_info.index = queue_index;
    ++queue_generation;
}

int playlist_update_resume_info(const struct mp3entry *id3)
{
    global_status.resume_index = queue_index;
    if(id3 != NULL) {
        global_status.resume_elapsed = id3->elapsed;
        global_status.resume_offset = id3->offset;
    }
    return 0;
}

void playlist_start(int start_index, unsigned long elapsed,
                    unsigned long offset)
{
    int index = normalize_index(start_index, false);

    if(index < 0)
        return;

    queue_index = index;
    queue_started = true;
    queue_info.index = index;
    queue_info.started = true;
    audio_play(elapsed, offset);
    audio_resume();
}

int playlist_amount(void)
{
    return queue_length;
}

int playlist_get_display_index(void)
{
    return queue_length > 0 ? queue_index + 1 : 0;
}

int playlist_get_track_info(
    struct playlist_info *playlist, int index,
    struct playlist_track_info *info)
{
    (void)playlist;
    if(info == NULL || index < 0 || index >= queue_length)
        return -1;
    memset(info, 0, sizeof(*info));
    copy_path(info->filename, queue_paths[index]);
    info->index = index;
    info->display_index = index + 1;
    return 0;
}

struct playlist_info *playlist_get_current(void)
{
    return &queue_info;
}

bool playlist_allow_dirplay(const struct playlist_info *playlist)
{
    (void)playlist;
    return false;
}

int playlist_get_resume_info(int *resume_index)
{
    if(resume_index != NULL)
        *resume_index = queue_index;
    return queue_started ? 0 : -1;
}

void playlist_resume_track(int start_index, unsigned int crc,
                           unsigned long elapsed, unsigned long offset)
{
    (void)crc;
    playlist_start(start_index, elapsed, offset);
}

static void queue_replace(const char *const *paths, int count,
                          int start_index, bool preserve_selected)
{
    char selected[MAX_PATH];
    int i;

    audio_stop();
    queue_length = 0;
    queue_index = 0;
    queue_started = false;

    if(count > CRAZYPOD_QUEUE_CAPACITY)
        count = CRAZYPOD_QUEUE_CAPACITY;

    for(i = 0; i < count; ++i) {
        copy_path(queue_paths[i], paths[i]);
        copy_path(original_paths[i], paths[i]);
    }

    queue_length = count;
    queue_info.amount = count;
    queue_info.index = 0;
    queue_info.started = false;

    if(start_index < 0 || start_index >= queue_length)
        start_index = 0;
    if(queue_length > 0 && preserve_selected)
        copy_path(selected, queue_paths[start_index]);
    else
        selected[0] = '\0';

    if(queue_shuffle)
        crazypod_queue_set_shuffle(true);

    if(queue_shuffle && preserve_selected) {
        for(i = 0; i < queue_length; ++i) {
            if(strcmp(queue_paths[i], selected) == 0) {
                start_index = i;
                break;
            }
        }
    }
    ++queue_generation;
    playlist_start(start_index, 0, 0);
}

void crazypod_queue_replace(const char *const *paths, int count,
                            int start_index)
{
    queue_replace(paths, count, start_index, true);
}

void crazypod_queue_replace_shuffled(const char *const *paths, int count,
                                     unsigned int seed)
{
    shuffle_state ^= (uint32_t)seed + 0x9e3779b9u +
        (shuffle_state << 6) + (shuffle_state >> 2);
    queue_shuffle = true;
    global_settings.playlist_shuffle = true;
    queue_replace(paths, count, 0, false);
}

void crazypod_queue_restore_begin(void)
{
    queue_length = 0;
    queue_index = 0;
    queue_started = false;
    queue_shuffle = false;
    queue_info.amount = 0;
    queue_info.index = 0;
    queue_info.started = false;
}

bool crazypod_queue_restore_add(const char *path)
{
    if(path == NULL || path[0] != '/' ||
       path_is_excluded_ipod_music(path) ||
       queue_length >= CRAZYPOD_QUEUE_CAPACITY)
        return false;
    copy_path(queue_paths[queue_length], path);
    copy_path(original_paths[queue_length], path);
    ++queue_length;
    queue_info.amount = queue_length;
    return true;
}

void crazypod_queue_restore_finish(int selected_index, bool shuffled)
{
    if(selected_index < 0 || selected_index >= queue_length)
        selected_index = 0;
    queue_index = selected_index;
    queue_shuffle = shuffled;
    queue_info.index = selected_index;
    global_settings.playlist_shuffle = shuffled;
    ++queue_generation;
}

int crazypod_queue_count(void)
{
    return queue_length;
}

int crazypod_queue_index(void)
{
    return queue_index;
}

const char *crazypod_queue_path(int index)
{
    return index >= 0 && index < queue_length ? queue_paths[index] : NULL;
}

void crazypod_queue_set_shuffle(bool enabled)
{
    char current[MAX_PATH];
    int i;

    if(queue_length <= 0) {
        queue_shuffle = enabled;
        global_settings.playlist_shuffle = enabled;
        return;
    }

    copy_path(current, queue_paths[queue_index]);

    if(enabled) {
        for(i = queue_length - 1; i > 0; --i) {
            int candidate = (int)(next_random() % (uint32_t)(i + 1));
            char temporary[MAX_PATH];
            copy_path(temporary, queue_paths[i]);
            copy_path(queue_paths[i], queue_paths[candidate]);
            copy_path(queue_paths[candidate], temporary);
        }
    }
    else {
        for(i = 0; i < queue_length; ++i)
            copy_path(queue_paths[i], original_paths[i]);
    }

    for(i = 0; i < queue_length; ++i) {
        if(strcmp(queue_paths[i], current) == 0) {
            queue_index = i;
            break;
        }
    }

    queue_info.index = queue_index;
    queue_shuffle = enabled;
    global_settings.playlist_shuffle = enabled;
    ++queue_generation;
}

bool crazypod_queue_shuffle(void)
{
    return queue_shuffle;
}

void crazypod_queue_set_repeat(int repeat_mode)
{
    if(repeat_mode < REPEAT_OFF || repeat_mode > REPEAT_ONE)
        repeat_mode = REPEAT_OFF;
    global_settings.repeat_mode = repeat_mode;
    ++queue_generation;
}

int crazypod_queue_repeat(void)
{
    return global_settings.repeat_mode;
}

unsigned crazypod_queue_generation(void)
{
    return queue_generation;
}

#endif
