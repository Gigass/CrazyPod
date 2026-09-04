#include "config.h"

#ifdef IPOD_6G

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "core_alloc.h"
#include "metadata.h"
#include "playlist.h"
#include "settings.h"
#include "iap-usb.h"

#include "crazypod_playlist.h"
#include "crazypod_state.h"

static int queue_storage_handle;
static int queue_capacity;
static char (*queue_paths)[MAX_PATH];
static char (*original_paths)[MAX_PATH];
static int queue_length;
static int queue_index;
static bool queue_started;
static bool queue_shuffle;
static uint32_t shuffle_state = 0x43505f36;
static unsigned queue_generation;
static struct playlist_info queue_info;
static void queue_set_shuffle_locked(bool enabled);

static void queue_lock(void)
{
    mutex_lock(&queue_info.mutex);
}

static void queue_unlock(void)
{
    mutex_unlock(&queue_info.mutex);
}

static bool queue_storage_reserve(int required)
{
    char (*new_paths)[MAX_PATH];
    char (*new_original_paths)[MAX_PATH];
    size_t row_bytes = sizeof(*queue_paths);
    size_t bytes;
    int new_capacity;
    int new_handle;

    if(required <= queue_capacity)
        return true;
    if(required <= 0 ||
       (size_t)required > SIZE_MAX / row_bytes / 2)
        return false;

    new_capacity = queue_capacity > 0 ? queue_capacity : 64;
    while(new_capacity < required) {
        if(new_capacity > INT_MAX / 2) {
            new_capacity = required;
            break;
        }
        new_capacity *= 2;
    }
    bytes = (size_t)new_capacity * row_bytes * 2;
    new_handle = core_alloc(bytes);
    if(new_handle <= 0)
        return false;
    core_pin(new_handle);
    new_paths = core_get_data(new_handle);
    new_original_paths = new_paths + new_capacity;
    if(queue_length > 0) {
        queue_paths = core_get_data(queue_storage_handle);
        original_paths = queue_paths + queue_capacity;
        memcpy(new_paths, queue_paths,
               (size_t)queue_length * row_bytes);
        memcpy(new_original_paths, original_paths,
               (size_t)queue_length * row_bytes);
    }
    if(queue_storage_handle > 0) {
        core_unpin(queue_storage_handle);
        queue_storage_handle = core_free(queue_storage_handle);
    }
    queue_storage_handle = new_handle;
    queue_capacity = new_capacity;
    queue_paths = new_paths;
    original_paths = new_original_paths;
    return true;
}

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

static bool path_is_disabled_ipod_music(const char *path)
{
    static const char directory[] = "/iPod_Control/Music";
    size_t length = sizeof(directory) - 1;

    return !crazypod_state_read_ipod_music() && path != NULL &&
        strncmp(path, directory, length) == 0 &&
        (path[length] == '\0' || path[length] == '/');
}

void playlist_init(void)
{
    if(queue_storage_handle > 0) {
        core_unpin(queue_storage_handle);
        queue_storage_handle = core_free(queue_storage_handle);
    }
    queue_capacity = 0;
    queue_paths = NULL;
    original_paths = NULL;
    memset(&queue_info, 0, sizeof(queue_info));
    mutex_init(&queue_info.mutex);
    queue_info.index = 0;
    queue_info.max_playlist_size = INT_MAX;
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
    queue_lock();
    queue_length = 0;
    queue_index = 0;
    queue_started = false;
    queue_info.amount = 0;
    queue_info.index = 0;
    queue_info.started = false;
    ++queue_generation;
    queue_unlock();
    iap_on_tracks_count(0);
    return 0;
}

int playlist_insert_track(struct playlist_info *playlist, const char *filename,
                          int position, bool queued, bool sync)
{
    int count;
    int insert_at;
    int i;

    (void)playlist;
    (void)queued;
    (void)sync;

    queue_lock();
    if(filename == NULL || path_is_disabled_ipod_music(filename) ||
       queue_length == INT_MAX ||
       !queue_storage_reserve(queue_length + 1)) {
        queue_unlock();
        return -1;
    }

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
    count = queue_length;
    queue_info.amount = queue_length;
    ++queue_generation;
    queue_unlock();
    iap_on_tracks_count(count);
    return insert_at;
}

const char *playlist_peek(int steps, char *buffer, size_t buffer_size)
{
    int index;
    const char *path;

    queue_lock();
    index = normalize_index(queue_index + steps, true);
    if(index < 0) {
        queue_unlock();
        return NULL;
    }

    path = queue_paths[index];
    if(buffer != NULL && buffer_size > 0) {
        snprintf(buffer, buffer_size, "%s", path);
        queue_unlock();
        return buffer;
    }

    /* In this build all no-buffer callers only test existence. Returning a
     * queue pointer after releasing the mutex would reintroduce a lifetime
     * race with shuffle/reallocation. */
    queue_unlock();
    return (const char *)1;
}

bool playlist_check(int steps)
{
    bool valid;

    queue_lock();
    valid = normalize_index(queue_index + steps, false) >= 0;
    queue_unlock();
    return valid;
}

int playlist_next(int steps)
{
    int next;

    queue_lock();
    if(global_settings.repeat_mode == REPEAT_ONE && steps != 0)
        next = queue_index;
    else
        next = normalize_index(queue_index + steps, true);

    if(next < 0) {
        queue_unlock();
        return -1;
    }

    queue_index = next;
    queue_info.index = next;
    queue_unlock();
    return next;
}

bool playlist_next_dir(int direction)
{
    (void)direction;
    return false;
}

void playlist_skip_entry(struct playlist_info *playlist, int steps)
{
    int count;
    int index;
    int i;

    (void)playlist;
    queue_lock();
    index = normalize_index(queue_index + steps, false);
    if(index < 0) {
        queue_unlock();
        return;
    }

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
    count = queue_length;
    queue_unlock();
    iap_on_tracks_count(count);
}

int playlist_update_resume_info(const struct mp3entry *id3)
{
    queue_lock();
    global_status.resume_index = queue_index;
    if(id3 != NULL) {
        global_status.resume_elapsed = id3->elapsed;
        global_status.resume_offset = id3->offset;
    }
    queue_unlock();
    return 0;
}

void playlist_start(int start_index, unsigned long elapsed,
                    unsigned long offset)
{
    int index;

    queue_lock();
    index = normalize_index(start_index, false);
    if(index < 0) {
        queue_unlock();
        return;
    }

    queue_index = index;
    queue_started = true;
    queue_info.index = index;
    queue_info.started = true;
    queue_unlock();
    audio_play(elapsed, offset);
    audio_resume();
}

int playlist_amount(void)
{
    int amount;

    queue_lock();
    amount = queue_length;
    queue_unlock();
    return amount;
}

int playlist_get_display_index(void)
{
    int index;

    queue_lock();
    index = queue_length > 0 ? queue_index + 1 : 0;
    queue_unlock();
    return index;
}

int playlist_get_track_info(
    struct playlist_info *playlist, int index,
    struct playlist_track_info *info)
{
    (void)playlist;
    if(info == NULL)
        return -1;
    queue_lock();
    if(index < 0 || index >= queue_length) {
        queue_unlock();
        return -1;
    }
    memset(info, 0, sizeof(*info));
    copy_path(info->filename, queue_paths[index]);
    info->index = index;
    info->display_index = index + 1;
    queue_unlock();
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
    int result;

    queue_lock();
    if(resume_index != NULL)
        *resume_index = queue_index;
    result = queue_started ? 0 : -1;
    queue_unlock();
    return result;
}

void playlist_resume_track(int start_index, unsigned int crc,
                           unsigned long elapsed, unsigned long offset)
{
    (void)crc;
    playlist_start(start_index, elapsed, offset);
}

static bool queue_replace(const char *const *paths, int count,
                          int start_index, bool preserve_selected,
                          bool force_shuffle, unsigned int seed)
{
    char selected[MAX_PATH];
    bool previous_shuffle;
    uint32_t previous_shuffle_state;
    bool previous_audio_active;
    bool previous_audio_paused;
    unsigned long previous_elapsed = 0;
    unsigned long previous_offset = 0;
    bool previous_queue_started;
    bool start_playback;
    int i;

    if(count < 0 || (count > 0 && paths == NULL))
        return false;
    for(i = 0; i < count; ++i) {
        if(paths[i] == NULL || paths[i][0] != '/')
            return false;
    }
    {
        int status = audio_status();
        struct mp3entry *id3 = audio_current_track();

        previous_audio_active = (status & AUDIO_STATUS_PLAY) != 0;
        previous_audio_paused = (status & AUDIO_STATUS_PAUSE) != 0;
        if(previous_audio_active && id3 != NULL) {
            previous_elapsed = id3->elapsed;
            previous_offset = id3->offset;
        }
    }
    queue_lock();
    previous_queue_started = queue_started && queue_length > 0;
    queue_unlock();
    audio_stop();
    queue_lock();
    previous_shuffle = queue_shuffle;
    previous_shuffle_state = shuffle_state;
    if(force_shuffle) {
        shuffle_state ^= (uint32_t)seed + 0x9e3779b9u +
            (shuffle_state << 6) + (shuffle_state >> 2);
        queue_shuffle = true;
        global_settings.playlist_shuffle = true;
    }
    if(count > 0 && !queue_storage_reserve(count)) {
        if(force_shuffle) {
            shuffle_state = previous_shuffle_state;
            queue_shuffle = previous_shuffle;
            global_settings.playlist_shuffle = previous_shuffle;
        }
        queue_unlock();
        if(previous_audio_active && previous_queue_started) {
            audio_play(previous_elapsed, previous_offset);
            if(previous_audio_paused)
                audio_pause();
            else
                audio_resume();
        }
        return false;
    }
    queue_length = 0;
    queue_index = 0;
    queue_started = false;

    for(i = 0; i < count; ++i) {
        copy_path(queue_paths[i], paths[i]);
        if(queue_shuffle)
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
        queue_set_shuffle_locked(true);

    if(queue_shuffle && preserve_selected) {
        for(i = 0; i < queue_length; ++i) {
            if(strcmp(queue_paths[i], selected) == 0) {
                start_index = i;
                break;
            }
        }
    }
    ++queue_generation;
    start_playback = queue_length > 0;
    if(start_playback) {
        queue_index = start_index;
        queue_started = true;
        queue_info.index = start_index;
        queue_info.started = true;
    }
    queue_unlock();
    iap_on_tracks_count(count);
    iap_on_shuffle_state(crazypod_queue_shuffle());
    if(start_playback) {
        audio_play(0, 0);
        audio_resume();
    }
    return true;
}

bool crazypod_queue_replace(const char *const *paths, int count,
                            int start_index)
{
    return queue_replace(paths, count, start_index, true, false, 0);
}

bool crazypod_queue_replace_shuffled(const char *const *paths, int count,
                                     unsigned int seed)
{
    return queue_replace(paths, count, 0, false, true, seed);
}

void crazypod_queue_restore_begin(void)
{
    queue_lock();
    queue_length = 0;
    queue_index = 0;
    queue_started = false;
    queue_shuffle = false;
    queue_info.amount = 0;
    queue_info.index = 0;
    queue_info.started = false;
    queue_unlock();
}

bool crazypod_queue_restore_add(const char *path)
{
    bool added = false;

    queue_lock();
    if(path == NULL || path[0] != '/' ||
       path_is_disabled_ipod_music(path) ||
       queue_length == INT_MAX ||
       !queue_storage_reserve(queue_length + 1)) {
        queue_unlock();
        return false;
    }
    copy_path(queue_paths[queue_length], path);
    copy_path(original_paths[queue_length], path);
    ++queue_length;
    queue_info.amount = queue_length;
    added = true;
    queue_unlock();
    return added;
}

void crazypod_queue_restore_finish(int selected_index, bool shuffled)
{
    queue_lock();
    if(selected_index < 0 || selected_index >= queue_length)
        selected_index = 0;
    queue_index = selected_index;
    queue_shuffle = shuffled;
    queue_info.index = selected_index;
    global_settings.playlist_shuffle = shuffled;
    ++queue_generation;
    queue_unlock();
    iap_on_tracks_count(playlist_amount());
    iap_on_shuffle_state(shuffled);
}

int crazypod_queue_count(void)
{
    return playlist_amount();
}

int crazypod_queue_index(void)
{
    int index;

    queue_lock();
    index = queue_index;
    queue_unlock();
    return index;
}

bool crazypod_queue_copy_path(int index, char *buffer,
                              size_t buffer_size)
{
    bool copied = false;

    if(buffer == NULL || buffer_size == 0)
        return false;
    buffer[0] = '\0';
    queue_lock();
    if(index >= 0 && index < queue_length) {
        snprintf(buffer, buffer_size, "%s", queue_paths[index]);
        copied = true;
    }
    queue_unlock();
    return copied;
}

static void queue_set_shuffle_locked(bool enabled)
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
        if(!queue_shuffle) {
            for(i = 0; i < queue_length; ++i)
                copy_path(original_paths[i], queue_paths[i]);
        }
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

void crazypod_queue_set_shuffle(bool enabled)
{
    queue_lock();
    queue_set_shuffle_locked(enabled);
    queue_unlock();
    iap_on_shuffle_state(enabled);
}

bool crazypod_queue_shuffle(void)
{
    bool shuffled;

    queue_lock();
    shuffled = queue_shuffle;
    queue_unlock();
    return shuffled;
}

void crazypod_queue_set_repeat(int repeat_mode)
{
    if(repeat_mode < REPEAT_OFF || repeat_mode > REPEAT_ONE)
        repeat_mode = REPEAT_OFF;
    queue_lock();
    global_settings.repeat_mode = repeat_mode;
    ++queue_generation;
    queue_unlock();
    iap_on_repeat_state(repeat_mode);
}

int crazypod_queue_repeat(void)
{
    int repeat_mode;

    queue_lock();
    repeat_mode = global_settings.repeat_mode;
    queue_unlock();
    return repeat_mode;
}

unsigned crazypod_queue_generation(void)
{
    unsigned generation;

    queue_lock();
    generation = queue_generation;
    queue_unlock();
    return generation;
}

#endif
