#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <string.h>

#include "audio.h"
#include "dir.h"
#include "file.h"
#include "kernel.h"
#include "metadata.h"
#include "settings.h"
#include "sound.h"

#include "crazypod_playlist.h"
#include "crazypod_state.h"

#define STATE_DIRECTORY "/.crazypod"
#define STATE_PATH STATE_DIRECTORY "/state.bin"
#define STATE_TEMP_PATH STATE_DIRECTORY "/state.tmp"
#define QUEUE_PATH STATE_DIRECTORY "/queue.m3u8"
#define QUEUE_TEMP_PATH STATE_DIRECTORY "/queue.tmp"
#define STATE_MAGIC 0x43505354u
#define STATE_VERSION 1u
#define STATE_SAVE_INTERVAL (30 * HZ)

struct crazypod_state_disk {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    int32_t volume;
    int32_t repeat_mode;
    uint32_t shuffled;
    int32_t queue_index;
    uint32_t queue_count;
    uint32_t queue_hash;
    uint32_t elapsed;
    uint32_t checksum;
};

static unsigned long resume_elapsed;
static unsigned long last_saved_elapsed;
static long last_save_tick;
static unsigned saved_queue_generation;
static bool state_dirty;

static uint32_t hash_bytes(uint32_t hash, const void *data, size_t size)
{
    const unsigned char *bytes = data;
    size_t i;

    for(i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t state_checksum(const struct crazypod_state_disk *state)
{
    struct crazypod_state_disk copy = *state;
    copy.checksum = 0;
    return hash_bytes(2166136261u, &copy, sizeof(copy));
}

static bool read_exact(int fd, void *buffer, size_t size)
{
    unsigned char *bytes = buffer;
    size_t done = 0;

    while(done < size) {
        ssize_t count = read(fd, bytes + done, size - done);
        if(count <= 0)
            return false;
        done += (size_t)count;
    }
    return true;
}

static bool write_exact(int fd, const void *buffer, size_t size)
{
    const unsigned char *bytes = buffer;
    size_t done = 0;

    while(done < size) {
        ssize_t count = write(fd, bytes + done, size - done);
        if(count <= 0)
            return false;
        done += (size_t)count;
    }
    return true;
}

static int read_line(int fd, char *line, size_t size)
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

static bool load_header(struct crazypod_state_disk *state)
{
    int fd = open(STATE_PATH, O_RDONLY);
    bool valid;

    if(fd < 0)
        return false;
    valid = read_exact(fd, state, sizeof(*state));
    close(fd);

    return valid &&
           state->magic == STATE_MAGIC &&
           state->version == STATE_VERSION &&
           state->size == sizeof(*state) &&
           state->checksum == state_checksum(state);
}

static void clamp_and_apply_settings(const struct crazypod_state_disk *state)
{
    int volume = state->volume;
    int repeat = state->repeat_mode;

    if(volume < sound_min(SOUND_VOLUME))
        volume = sound_min(SOUND_VOLUME);
    if(volume > sound_max(SOUND_VOLUME))
        volume = sound_max(SOUND_VOLUME);
    if(repeat < REPEAT_OFF || repeat > REPEAT_ONE)
        repeat = REPEAT_OFF;

    global_status.volume = volume;
    global_settings.repeat_mode = repeat;
    sound_set_volume(volume);
}

void crazypod_state_load(void)
{
    struct crazypod_state_disk state;
    uint32_t queue_hash = 2166136261u;
    unsigned queue_count = 0;
    char line[MAX_PATH];
    int fd;

    resume_elapsed = 0;
    last_saved_elapsed = 0;
    last_save_tick = current_tick;
    saved_queue_generation = crazypod_queue_generation();
    state_dirty = false;

    if(!load_header(&state))
        return;

    clamp_and_apply_settings(&state);
    crazypod_queue_restore_begin();
    fd = open(QUEUE_PATH, O_RDONLY);
    if(fd >= 0) {
        while(read_line(fd, line, sizeof(line)) >= 0) {
            size_t length = strlen(line);
            if(length == 0 || line[0] != '/' || !file_exists(line))
                continue;
            if(crazypod_queue_restore_add(line)) {
                queue_hash = hash_bytes(queue_hash, line, length);
                queue_hash = hash_bytes(queue_hash, "\n", 1);
                ++queue_count;
            }
        }
        close(fd);
    }

    if(queue_count == state.queue_count && queue_hash == state.queue_hash) {
        crazypod_queue_restore_finish(state.queue_index,
                                      state.shuffled != 0);
        resume_elapsed = state.elapsed;
        last_saved_elapsed = state.elapsed;
    }
    else {
        crazypod_queue_restore_finish(0, false);
    }
    saved_queue_generation = crazypod_queue_generation();
}

void crazypod_state_mark_dirty(void)
{
    state_dirty = true;
}

void crazypod_state_forget_resume(void)
{
    resume_elapsed = 0;
    last_saved_elapsed = 0;
    state_dirty = true;
}

unsigned long crazypod_state_take_resume_elapsed(void)
{
    unsigned long elapsed = resume_elapsed;
    resume_elapsed = 0;
    return elapsed;
}

static bool save_queue(uint32_t *hash_out, uint32_t *count_out)
{
    uint32_t hash = 2166136261u;
    uint32_t count = 0;
    int fd;
    int i;
    bool success = true;

    mkdir(STATE_DIRECTORY);
    fd = open(QUEUE_TEMP_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;

    for(i = 0; i < crazypod_queue_count(); ++i) {
        const char *path = crazypod_queue_path(i);
        size_t length;
        if(path == NULL || path[0] != '/')
            continue;
        length = strlen(path);
        if(!write_exact(fd, path, length) ||
           !write_exact(fd, "\n", 1)) {
            success = false;
            break;
        }
        hash = hash_bytes(hash, path, length);
        hash = hash_bytes(hash, "\n", 1);
        ++count;
    }
    if(fsync(fd) < 0)
        success = false;
    close(fd);
    if(!success || rename(QUEUE_TEMP_PATH, QUEUE_PATH) < 0)
        return false;

    *hash_out = hash;
    *count_out = count;
    return true;
}

void crazypod_state_save(bool force)
{
    struct crazypod_state_disk state;
    struct mp3entry *id3;
    uint32_t queue_hash;
    uint32_t queue_count;
    unsigned long elapsed;
    int fd;
    bool success;

    id3 = audio_current_track();
    elapsed = id3 != NULL ? id3->elapsed : resume_elapsed;
    if(!force && !state_dirty &&
       crazypod_queue_generation() == saved_queue_generation &&
       elapsed / 30000 == last_saved_elapsed / 30000)
        return;

    if(!save_queue(&queue_hash, &queue_count))
        return;

    memset(&state, 0, sizeof(state));
    state.magic = STATE_MAGIC;
    state.version = STATE_VERSION;
    state.size = sizeof(state);
    state.volume = global_status.volume;
    state.repeat_mode = crazypod_queue_repeat();
    state.shuffled = crazypod_queue_shuffle();
    state.queue_index = crazypod_queue_index();
    state.queue_count = queue_count;
    state.queue_hash = queue_hash;
    state.elapsed = elapsed;
    state.checksum = state_checksum(&state);

    fd = open(STATE_TEMP_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return;
    success = write_exact(fd, &state, sizeof(state));
    if(fsync(fd) < 0)
        success = false;
    close(fd);
    if(!success || rename(STATE_TEMP_PATH, STATE_PATH) < 0)
        return;

    state_dirty = false;
    last_saved_elapsed = elapsed;
    last_save_tick = current_tick;
    saved_queue_generation = crazypod_queue_generation();
}

void crazypod_state_tick(void)
{
    if(TIME_AFTER(current_tick, last_save_tick + STATE_SAVE_INTERVAL))
        crazypod_state_save(false);
}

#endif
