#include "config.h"

#ifdef IPOD_6G

#define CRAZYPOD_VIDEO_CORE 1

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef SIMULATOR
#include <stdlib.h>
#endif

#include "audio.h"
#include "bmp.h"
#include "buflib.h"
#include "button.h"
#include "core_alloc.h"
#include "dir.h"
#include "file.h"
#include "kernel.h"
#include "lcd.h"
#include "pathfuncs.h"
#include "pcm_mixer.h"
#include "pcmbuf.h"
#include "powermgmt.h"
#include "settings.h"
#include "sound.h"
#include "storage.h"
#include "system.h"
#include "usb.h"
#ifdef SIMULATOR
#include "screendump.h"
#endif

#include "crazypod_video_plugin.h"
#include "../plugins/mpegplayer/mpegplayer.h"
#include "../plugins/mpegplayer/mpeg_settings.h"

#ifdef HAVE_SCHEDULER_BOOSTCTRL
#undef trigger_cpu_boost
#undef cancel_cpu_boost
#endif

#include "crazypod_image.h"
#include "crazypod_lcd.h"
#include "crazypod_state.h"
#include "crazypod_videos.h"

struct mpeg_settings settings = {
    .limitfps = 1,
    .skipframes = 1,
};

#define CRAZYPOD_VIDEO_DIRECTORY "/Videos"
#define CRAZYPOD_VIDEO_DIRECTORY_DEPTH 4
#define CRAZYPOD_VIDEO_STATE_DIRECTORY "/.crazypod"
#define CRAZYPOD_VIDEO_STATE_PATH \
    CRAZYPOD_VIDEO_STATE_DIRECTORY "/video-resume.bin"
#define CRAZYPOD_VIDEO_STATE_TMP \
    CRAZYPOD_VIDEO_STATE_DIRECTORY "/video-resume.tmp"
#define CRAZYPOD_VIDEO_STATE_MAGIC 0x43505652u
#define CRAZYPOD_VIDEO_STATE_VERSION 1u
#define CRAZYPOD_VIDEO_POSTER_EXTRA (64u * 1024u)
#define CRAZYPOD_VIDEO_POSTER_STACK_SIZE 0x3000
#define CRAZYPOD_VIDEO_POSTER_WAKE 1
#define CRAZYPOD_VIDEO_UI_HEIGHT 42
#define CRAZYPOD_VIDEO_SEEK_SECONDS 10u
#define CRAZYPOD_VIDEO_RESUME_MIN_SECONDS 5u
#define CRAZYPOD_VIDEO_RESUME_END_SECONDS 5u
#define CRAZYPOD_VIDEO_CLOCK_CHECK_SECONDS 2u

struct video_entry {
    char path[MAX_PATH];
    char poster_path[MAX_PATH];
    char name[MAX_PATH];
    uint32_t size;
    uint32_t mtime;
    uint32_t resume_ticks;
    uint32_t duration_ticks;
};

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

struct video_poster_slot {
    fb_data pixels[2][CRAZYPOD_VIDEO_POSTER_WIDTH *
                      CRAZYPOD_VIDEO_POSTER_HEIGHT]
        CACHEALIGN_AT_LEAST_ATTR(16);
    lv_image_dsc_t descriptor[2];
    char requested_path[MAX_PATH];
    unsigned request_serial;
    unsigned decoded_serial;
    int requested_index;
    int decoded_index;
    int active_bank;
    bool pending;
    bool valid;
};

static struct video_entry videos[CRAZYPOD_VIDEO_MAX_FILES];
static int video_count;
static struct video_poster_slot poster_slot;
static struct mutex video_mutex;
static struct event_queue video_queue;
static long video_poster_stack[
    CRAZYPOD_VIDEO_POSTER_STACK_SIZE / sizeof(long)];
static bool video_poster_suspended;
static bool video_poster_decoding;
static bool video_poster_wake_queued;
static unsigned video_generation;
static enum crazypod_video_result last_video_result = CRAZYPOD_VIDEO_OK;
static int video_audio_buffer_handle;
static size_t video_audio_buffer_size;
static bool video_buffer_allocation_failed;
static char video_engine_message[96];

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

static bool video_path_supported(const char *path)
{
    const char *extension = strrchr(path, '.');

    return extension != NULL &&
        (strcasecmp(extension, ".mpg") == 0 ||
         strcasecmp(extension, ".mpeg") == 0);
}

static int compare_video_entries(const struct video_entry *left,
                                 const struct video_entry *right)
{
    int result = strcasecmp(left->name, right->name);

    return result != 0 ? result : strcasecmp(left->path, right->path);
}

static void video_name_from_path(char *name, size_t size, const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *base = slash != NULL ? slash + 1 : path;
    char *extension;

    snprintf(name, size, "%s", base);
    extension = strrchr(name, '.');
    if(extension != NULL)
        *extension = '\0';
}

static void video_poster_path(char *poster, size_t size, const char *path)
{
    char *extension;

    snprintf(poster, size, "%s", path);
    extension = strrchr(poster, '.');
    if(extension != NULL)
        snprintf(extension, size - (size_t)(extension - poster), ".bmp");
}

static void insert_video(const char *path, const struct dirinfo *info)
{
    struct video_entry entry;
    int position;

    if(video_count >= CRAZYPOD_VIDEO_MAX_FILES)
        return;
    memset(&entry, 0, sizeof(entry));
    snprintf(entry.path, sizeof(entry.path), "%s", path);
    video_poster_path(entry.poster_path, sizeof(entry.poster_path), path);
    video_name_from_path(entry.name, sizeof(entry.name), path);
    entry.size = info->size;
    entry.mtime = info->mtime;
    position = video_count;
    while(position > 0 &&
          compare_video_entries(&entry, &videos[position - 1]) < 0) {
        videos[position] = videos[position - 1];
        --position;
    }
    videos[position] = entry;
    ++video_count;
}

static void scan_video_directory(const char *path, int depth)
{
    DIR *directory;
    struct dirent *entry;

    if(depth > CRAZYPOD_VIDEO_DIRECTORY_DEPTH ||
       video_count >= CRAZYPOD_VIDEO_MAX_FILES)
        return;
    directory = opendir(path);
    if(directory == NULL)
        return;
    while(video_count < CRAZYPOD_VIDEO_MAX_FILES &&
          (entry = readdir(directory)) != NULL) {
        struct dirinfo info = dir_get_info(directory, entry);
        char child[MAX_PATH];

        /*
         * FAT volumes mounted by macOS can contain AppleDouble sidecars such
         * as "._movie.mpg". They carry metadata, not MPEG data, but their
         * suffix made them look playable and sorted them before the real
         * video. Ignore every hidden path component at the scan boundary.
         */
        if(entry->d_name[0] == '.')
            continue;
        if(path_append(child, path, entry->d_name, sizeof(child)) >=
           (int)sizeof(child))
            continue;
        if(info.attribute & ATTR_DIRECTORY)
            scan_video_directory(child, depth + 1);
        else if(video_path_supported(child))
            insert_video(child, &info);
        yield();
    }
    closedir(directory);
}

static int video_index_for_path(const char *path, uint32_t size,
                                uint32_t mtime)
{
    int index;

    for(index = 0; index < video_count; ++index) {
        if(videos[index].size == size &&
           videos[index].mtime == mtime &&
           strcmp(videos[index].path, path) == 0)
            return index;
    }
    return -1;
}

static void load_video_state(void)
{
    struct video_resume_file_header header;
    int fd = open(CRAZYPOD_VIDEO_STATE_PATH, O_RDONLY);
    uint32_t record;

    if(fd < 0)
        return;
    if(!read_exact(fd, &header, sizeof(header)) ||
       header.magic != CRAZYPOD_VIDEO_STATE_MAGIC ||
       header.version != CRAZYPOD_VIDEO_STATE_VERSION ||
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
        disk_entry.path[MAX_PATH - 1] = '\0';
        index = video_index_for_path(
            disk_entry.path, disk_entry.size, disk_entry.mtime);
        if(index >= 0) {
            videos[index].resume_ticks = disk_entry.resume_ticks;
            videos[index].duration_ticks = disk_entry.duration_ticks;
        }
    }
    close(fd);
}

static bool save_video_state(void)
{
    struct video_resume_file_header header;
    int fd;
    int index;
    bool complete = true;

    mkdir(CRAZYPOD_VIDEO_STATE_DIRECTORY);
    fd = open(CRAZYPOD_VIDEO_STATE_TMP,
              O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    header.magic = CRAZYPOD_VIDEO_STATE_MAGIC;
    header.version = CRAZYPOD_VIDEO_STATE_VERSION;
    header.entry_size = sizeof(struct video_resume_disk_entry);
    header.count = (uint32_t)video_count;
    complete = write_exact(fd, &header, sizeof(header));
    for(index = 0; complete && index < video_count; ++index) {
        struct video_resume_disk_entry disk_entry;

        memset(&disk_entry, 0, sizeof(disk_entry));
        memcpy(disk_entry.path, videos[index].path,
               sizeof(disk_entry.path));
        disk_entry.path[sizeof(disk_entry.path) - 1] = '\0';
        disk_entry.size = videos[index].size;
        disk_entry.mtime = videos[index].mtime;
        disk_entry.resume_ticks = videos[index].resume_ticks;
        disk_entry.duration_ticks = videos[index].duration_ticks;
        complete = write_exact(fd, &disk_entry, sizeof(disk_entry));
    }
    if(complete)
        complete = fsync(fd) >= 0;
    close(fd);
    if(!complete) {
        remove(CRAZYPOD_VIDEO_STATE_TMP);
        return false;
    }
    if(rename(CRAZYPOD_VIDEO_STATE_TMP,
              CRAZYPOD_VIDEO_STATE_PATH) < 0) {
        remove(CRAZYPOD_VIDEO_STATE_TMP);
        return false;
    }
    return true;
}

static bool decode_video_poster(const char *path,
                                lv_image_dsc_t *descriptor,
                                fb_data *destination)
{
    struct bitmap bitmap;
    size_t pixel_bytes =
        (size_t)CRAZYPOD_VIDEO_POSTER_WIDTH *
        CRAZYPOD_VIDEO_POSTER_HEIGHT * sizeof(fb_data);
    size_t decode_bytes = pixel_bytes + CRAZYPOD_VIDEO_POSTER_EXTRA;
    int handle;
    fb_data *decode_buffer;
    int result;
    int row;

    if(!file_exists(path))
        return false;
    handle = core_alloc_ex(decode_bytes, &buflib_ops_locked);
    if(handle < 0)
        return false;
    decode_buffer = core_get_data(handle);
    memset(&bitmap, 0, sizeof(bitmap));
    bitmap.width = CRAZYPOD_VIDEO_POSTER_WIDTH;
    bitmap.height = CRAZYPOD_VIDEO_POSTER_HEIGHT;
    bitmap.data = (unsigned char *)decode_buffer;
    crazypod_image_decode_lock();
    result = read_bmp_file(
        path, &bitmap, decode_bytes,
        FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT,
        &format_native);
    crazypod_image_decode_unlock();
    if(result < 0 || bitmap.width <= 0 || bitmap.height <= 0 ||
       bitmap.width > CRAZYPOD_VIDEO_POSTER_WIDTH ||
       bitmap.height > CRAZYPOD_VIDEO_POSTER_HEIGHT ||
       bitmap.data == NULL) {
        core_free(handle);
        return false;
    }
    for(row = 0; row < bitmap.height; ++row) {
        memcpy(destination + row * bitmap.width,
               (fb_data *)bitmap.data + row * bitmap.width,
               (size_t)bitmap.width * sizeof(fb_data));
    }
    core_free(handle);
    return crazypod_image_configure_rgb565(
        descriptor, destination, bitmap.width, bitmap.height);
}

static void video_poster_thread(void)
{
    for(;;) {
        struct queue_event event;

        queue_wait(&video_queue, &event);
        if(event.id != CRAZYPOD_VIDEO_POSTER_WAKE)
            continue;
        for(;;) {
            char path[MAX_PATH];
            unsigned serial;
            int index;
            int bank;
            bool valid;

            mutex_lock(&video_mutex);
            if(video_poster_suspended || !poster_slot.pending) {
                video_poster_decoding = false;
                video_poster_wake_queued = false;
                mutex_unlock(&video_mutex);
                break;
            }
            snprintf(path, sizeof(path), "%s",
                     poster_slot.requested_path);
            serial = poster_slot.request_serial;
            index = poster_slot.requested_index;
            bank = 1 - poster_slot.active_bank;
            poster_slot.pending = false;
            video_poster_decoding = true;
            mutex_unlock(&video_mutex);

            valid = decode_video_poster(
                path, &poster_slot.descriptor[bank],
                poster_slot.pixels[bank]);

            mutex_lock(&video_mutex);
            if(serial == poster_slot.request_serial &&
               index == poster_slot.requested_index &&
               strcmp(path, poster_slot.requested_path) == 0) {
                poster_slot.decoded_index = index;
                poster_slot.decoded_serial = serial;
                poster_slot.active_bank = bank;
                poster_slot.valid = valid;
                ++video_generation;
            }
            video_poster_decoding = false;
            mutex_unlock(&video_mutex);
        }
    }
}

static void wake_video_poster_worker(void)
{
    bool post = false;

    mutex_lock(&video_mutex);
    if(!video_poster_suspended && !video_poster_wake_queued) {
        video_poster_wake_queued = true;
        post = true;
    }
    mutex_unlock(&video_mutex);
    if(post)
        queue_post(&video_queue, CRAZYPOD_VIDEO_POSTER_WAKE, 0);
}

static void video_engine_splashf(int ticks, const char *format, ...)
{
    va_list arguments;

    (void)ticks;
    va_start(arguments, format);
    vsnprintf(video_engine_message, sizeof(video_engine_message),
              format, arguments);
    va_end(arguments);
}

static int video_engine_open(const char *path, int flags)
{
    return open(path, flags);
}

#ifndef CPU_BOOST_LOGGING
static void video_engine_cpu_boost(bool enabled)
{
    (void)enabled;
    cpu_boost(enabled);
}
#endif

static void video_engine_storage_sleep(void)
{
    storage_sleep();
}

static void video_engine_storage_spin(void)
{
    storage_spin();
}

static void *video_engine_audio_buffer(size_t *size)
{
    if(video_audio_buffer_handle <= 0) {
        audio_stop();
        video_audio_buffer_handle = core_alloc_maximum(
            &video_audio_buffer_size, &buflib_ops_locked);
        if(video_audio_buffer_handle <= 0)
            video_buffer_allocation_failed = true;
    }
    if(size != NULL)
        *size = video_audio_buffer_size;
    return video_audio_buffer_handle > 0
        ? core_get_data(video_audio_buffer_handle) : NULL;
}

static void video_engine_release_audio_buffer(void)
{
    if(video_audio_buffer_handle > 0)
        video_audio_buffer_handle =
            core_free(video_audio_buffer_handle);
    video_audio_buffer_size = 0;
}

static const struct crazypod_video_plugin_api video_engine_api = {
    .splash = video_engine_splashf,
    .splashf = video_engine_splashf,
    .lcd_update = lcd_update,
    .lcd_clear_display = lcd_clear_display,
    .lcd_update_rect = lcd_update_rect,
    .lcd_fillrect = lcd_fillrect,
    .lcd_set_foreground = lcd_set_foreground,
    .lcd_get_foreground = lcd_get_foreground,
    .lcd_set_background = lcd_set_background,
    .lcd_get_background = lcd_get_background,
    .lcd_blit_yuv = lcd_blit_yuv,
    .open = video_engine_open,
    .close = close,
    .read = read,
    .lseek = lseek,
    .filesize = filesize,
    .storage_sleep = video_engine_storage_sleep,
    .storage_spin = video_engine_storage_spin,
    .stub_storage_sleep = video_engine_storage_sleep,
    .stub_storage_spin = video_engine_storage_spin,
    .ata_spin = video_engine_storage_spin,
    .yield = yield,
    .create_thread = create_thread,
    .thread_self = thread_self,
    .thread_wait = thread_wait,
#ifdef HAVE_PRIORITY_SCHEDULING
    .thread_set_priority = thread_set_priority,
#endif
    .mutex_init = mutex_init,
    .mutex_lock = mutex_lock,
    .mutex_unlock = mutex_unlock,
#ifdef CPU_BOOST_LOGGING
    .cpu_boost_ = cpu_boost_,
#else
    .cpu_boost = video_engine_cpu_boost,
#endif
#ifdef HAVE_SCHEDULER_BOOSTCTRL
    .trigger_cpu_boost = trigger_cpu_boost,
    .cancel_cpu_boost = cancel_cpu_boost,
#endif
    .commit_dcache = commit_dcache,
    .commit_discard_dcache = commit_discard_dcache,
    .queue_init = queue_init,
    .queue_post = queue_post,
    .queue_wait_w_tmo = queue_wait_w_tmo,
    .queue_enable_queue_send = queue_enable_queue_send,
    .queue_wait = queue_wait,
    .queue_empty = queue_empty,
    .queue_send = queue_send,
    .queue_reply = queue_reply,
    .current_tick = &current_tick,
    .memset = memset,
    .memcpy = memcpy,
    .memmove = memmove,
    .memcmp = memcmp,
#ifdef HAVE_PITCHCONTROL
    .sound_set_pitch = sound_set_pitch,
    .dsp_set_timestretch = dsp_set_timestretch,
#endif
    .pcm_play_lock = pcm_play_lock,
    .pcm_play_unlock = pcm_play_unlock,
    .dsp_configure = dsp_configure,
    .dsp_get_config = dsp_get_config,
    .dsp_process = dsp_process,
#if INPUT_SRC_CAPS != 0
    .audio_set_output_source = audio_set_output_source,
    .audio_set_input_source = audio_set_input_source,
#endif
    .mixer_channel_status = mixer_channel_status,
    .mixer_channel_play_data = mixer_channel_play_data,
    .mixer_channel_play_pause = mixer_channel_play_pause,
    .mixer_channel_stop = mixer_channel_stop,
    .mixer_channel_set_amplitude = mixer_channel_set_amplitude,
    .mixer_channel_get_bytes_waiting =
        mixer_channel_get_bytes_waiting,
    .mixer_set_frequency = mixer_set_frequency,
    .mixer_get_frequency = mixer_get_frequency,
    .pcmbuf_fade = pcmbuf_fade,
    .codec_thread_do_callback = codec_thread_do_callback,
    .plugin_get_audio_buffer = video_engine_audio_buffer,
};

const struct crazypod_video_plugin_api *rb = &video_engine_api;

static void draw_video_controls(const struct video_entry *video,
                                bool paused, const char *message)
{
    uint32_t duration = stream_get_duration();
    uint32_t elapsed = stream_get_time();

    crazypod_lcd_draw_video_controls(
        video->name, elapsed / TS_SECOND,
        duration / TS_SECOND, global_status.volume,
        paused, message);
}

static uint32_t valid_resume_time(const struct video_entry *video,
                                  uint32_t duration)
{
    uint32_t minimum = CRAZYPOD_VIDEO_RESUME_MIN_SECONDS * TS_SECOND;
    uint32_t end_margin = CRAZYPOD_VIDEO_RESUME_END_SECONDS * TS_SECOND;

    if(video->resume_ticks < minimum ||
       duration <= end_margin ||
       video->resume_ticks >= duration - end_margin)
        return 0;
    return video->resume_ticks;
}

static void adjust_video_volume(int direction)
{
    int next = global_status.volume + direction;

    if(next < sound_min(SOUND_VOLUME))
        next = sound_min(SOUND_VOLUME);
    if(next > sound_max(SOUND_VOLUME))
        next = sound_max(SOUND_VOLUME);
    if(next == global_status.volume)
        return;
    sound_set_volume(next);
    global_status.volume = next;
    crazypod_state_mark_dirty();
}

static uint32_t seek_target(int direction)
{
    uint32_t current = stream_get_time();
    uint32_t duration = stream_get_duration();
    uint32_t amount = CRAZYPOD_VIDEO_SEEK_SECONDS * TS_SECOND;

    if(direction < 0)
        return current > amount ? current - amount : 0;
    if(duration > amount && current < duration - amount)
        return current + amount;
    return duration > 0 ? duration - 1 : current;
}

void crazypod_videos_init(void)
{
    memset(videos, 0, sizeof(videos));
    memset(&poster_slot, 0, sizeof(poster_slot));
    poster_slot.requested_index = -1;
    poster_slot.decoded_index = -1;
    mutex_init(&video_mutex);
    queue_init(&video_queue, false);
    create_thread(video_poster_thread, video_poster_stack,
                  sizeof(video_poster_stack), 0,
                  "crazypod video posters"
                  IF_PRIO(, PRIORITY_USER_INTERFACE)
                  IF_COP(, CPU));
    mkdir(CRAZYPOD_VIDEO_DIRECTORY);
    mkdir(CRAZYPOD_VIDEO_STATE_DIRECTORY);
    crazypod_videos_refresh();
}

void crazypod_videos_refresh(void)
{
    crazypod_videos_suspend();
    video_count = 0;
    memset(videos, 0, sizeof(videos));
    scan_video_directory(CRAZYPOD_VIDEO_DIRECTORY, 0);
    load_video_state();
    mutex_lock(&video_mutex);
    memset(&poster_slot, 0, sizeof(poster_slot));
    poster_slot.requested_index = -1;
    poster_slot.decoded_index = -1;
    video_poster_suspended = false;
    video_poster_wake_queued = false;
    ++video_generation;
    mutex_unlock(&video_mutex);
}

void crazypod_videos_suspend(void)
{
    bool decoding;

    mutex_lock(&video_mutex);
    video_poster_suspended = true;
    mutex_unlock(&video_mutex);

    do {
        mutex_lock(&video_mutex);
        decoding = video_poster_decoding;
        mutex_unlock(&video_mutex);
        if(!decoding)
            break;
        yield();
    } while(true);
}

void crazypod_videos_resume(void)
{
    mutex_lock(&video_mutex);
    video_poster_suspended = false;
    video_poster_wake_queued = false;
    mutex_unlock(&video_mutex);
    wake_video_poster_worker();
}

int crazypod_video_count(void)
{
    return video_count;
}

const char *crazypod_video_path(int index)
{
    return index >= 0 && index < video_count ? videos[index].path : "";
}

const char *crazypod_video_name(int index)
{
    return index >= 0 && index < video_count ? videos[index].name : "";
}

uint32_t crazypod_video_resume_seconds(int index)
{
    return index >= 0 && index < video_count
        ? videos[index].resume_ticks / TS_SECOND : 0;
}

uint32_t crazypod_video_duration_seconds(int index)
{
    return index >= 0 && index < video_count
        ? videos[index].duration_ticks / TS_SECOND : 0;
}

const lv_image_dsc_t *crazypod_video_poster(int index)
{
    const lv_image_dsc_t *result = NULL;
    bool changed = false;

    if(index < 0 || index >= video_count)
        return NULL;
    mutex_lock(&video_mutex);
    if(poster_slot.decoded_index == index &&
       poster_slot.decoded_serial == poster_slot.request_serial &&
       poster_slot.valid &&
       strcmp(poster_slot.requested_path,
              videos[index].poster_path) == 0) {
        result = &poster_slot.descriptor[poster_slot.active_bank];
    }
    else if(poster_slot.requested_index != index ||
            strcmp(poster_slot.requested_path,
                   videos[index].poster_path) != 0) {
        poster_slot.requested_index = index;
        snprintf(poster_slot.requested_path,
                 sizeof(poster_slot.requested_path),
                 "%s", videos[index].poster_path);
        ++poster_slot.request_serial;
        poster_slot.pending = true;
        poster_slot.valid = false;
        changed = true;
    }
    mutex_unlock(&video_mutex);
    if(changed)
        wake_video_poster_worker();
    return result;
}

unsigned crazypod_video_generation(void)
{
    unsigned generation;

    mutex_lock(&video_mutex);
    generation = video_generation;
    mutex_unlock(&video_mutex);
    return generation;
}

bool crazypod_videos_busy(void)
{
    bool busy;

    mutex_lock(&video_mutex);
    busy = video_poster_decoding || poster_slot.pending;
    mutex_unlock(&video_mutex);
    return busy;
}

enum crazypod_video_result crazypod_video_play(int index)
{
    struct video_entry *video;
    struct vo_rect clip;
    enum crazypod_video_result result = CRAZYPOD_VIDEO_OK;
    uint32_t duration = 0;
    uint32_t resume = 0;
    uint32_t last_draw_tick = 0;
    uint32_t clock_check_time = 0;
    long clock_check_tick = 0;
    bool engine_initialized = false;
    bool stream_opened = false;
    bool paused = false;
    bool clock_checked = false;
    bool clock_stalled = false;
    bool completed = false;
    bool repost_system_event = false;
    int system_event = 0;
    intptr_t system_event_data = 0;
#ifdef SIMULATOR
    bool simulator_dump =
        getenv("CRAZYPOD_SIM_VIDEO_DUMP") != NULL;
    int simulator_dump_seconds = 1;
    const char *simulator_dump_after =
        getenv("CRAZYPOD_SIM_VIDEO_DUMP_AFTER");
    long simulator_dump_due;

    if(simulator_dump_after != NULL) {
        simulator_dump_seconds = atoi(simulator_dump_after);
        if(simulator_dump_seconds < 1)
            simulator_dump_seconds = 1;
        if(simulator_dump_seconds > 30)
            simulator_dump_seconds = 30;
    }
    simulator_dump_due =
        current_tick + simulator_dump_seconds * HZ;
#endif

    if(index < 0 || index >= video_count) {
        last_video_result = CRAZYPOD_VIDEO_INVALID_FILE;
        return last_video_result;
    }
    video = &videos[index];
    if(!video_path_supported(video->path)) {
        last_video_result = CRAZYPOD_VIDEO_UNSUPPORTED;
        return last_video_result;
    }

    crazypod_videos_suspend();
    video_engine_message[0] = '\0';
    video_buffer_allocation_failed = false;
    audio_stop();
    cpu_boost(true);
    lcd_clear_display();
    lcd_update();

    if(stream_init() < STREAM_OK) {
        result = video_buffer_allocation_failed
            ? CRAZYPOD_VIDEO_NO_MEMORY
            : CRAZYPOD_VIDEO_ENGINE_FAILED;
        goto cleanup;
    }
    engine_initialized = true;
    {
        int open_result = stream_open(video->path);

        if(open_result < STREAM_OK) {
            result = open_result == STREAM_UNSUPPORTED
                ? CRAZYPOD_VIDEO_UNSUPPORTED
                : CRAZYPOD_VIDEO_OPEN_FAILED;
            goto cleanup;
        }
    }
    stream_opened = true;
    duration = stream_get_duration();
    video->duration_ticks = duration;
    vo_rect_set_ext(&clip, 0, 0, LCD_WIDTH,
                    LCD_HEIGHT - CRAZYPOD_VIDEO_UI_HEIGHT);
    stream_vo_set_clip(&clip);
    stream_show_vo(true);
    resume = valid_resume_time(video, duration);
    if(resume > 0)
        stream_seek(resume, SEEK_SET);
    if(stream_play() < STREAM_OK) {
        result = CRAZYPOD_VIDEO_ENGINE_FAILED;
        goto cleanup;
    }
    clock_check_time = stream_get_time();
    clock_check_tick =
        current_tick + CRAZYPOD_VIDEO_CLOCK_CHECK_SECONDS * HZ;
    draw_video_controls(
        video, false, resume > 0 ? "RESUMED" : NULL);

    for(;;) {
        int status = stream_status();
        int button;
        int base;

        if(status == STREAM_STOPPED) {
            completed = true;
            break;
        }
        if(!clock_checked &&
           !TIME_BEFORE(current_tick, clock_check_tick)) {
            clock_checked = true;
            clock_stalled = stream_get_time() == clock_check_time;
        }
        if(!TIME_BEFORE(current_tick, last_draw_tick + HZ / 4)) {
            draw_video_controls(
                video, paused,
                clock_stalled ? "CLOCK STALLED" : NULL);
            last_draw_tick = current_tick;
        }
#ifdef SIMULATOR
        if(simulator_dump &&
           !TIME_BEFORE(current_tick, simulator_dump_due)) {
            screen_dump();
            break;
        }
#endif
        reset_poweroff_timer();
        button = button_get_w_tmo(HZ / 20);
        if(button == BUTTON_NONE)
            continue;
        if(button == SYS_USB_CONNECTED ||
           button == SYS_POWEROFF ||
           button == SYS_REBOOT) {
            repost_system_event = true;
            system_event = button;
            system_event_data = button_get_data();
            break;
        }
        base = button & ~(BUTTON_REL | BUTTON_REPEAT);
        if(base == BUTTON_MENU && (button & BUTTON_REL)) {
            break;
        }
        if(base == BUTTON_PLAY && (button & BUTTON_REL)) {
            if(paused) {
                stream_resume();
                paused = false;
                clock_check_time = stream_get_time();
                clock_check_tick =
                    current_tick +
                    CRAZYPOD_VIDEO_CLOCK_CHECK_SECONDS * HZ;
                clock_checked = false;
                clock_stalled = false;
            }
            else {
                stream_pause();
                paused = true;
            }
            draw_video_controls(video, paused, NULL);
        }
        else if((base == BUTTON_LEFT || base == BUTTON_RIGHT) &&
                !(button & BUTTON_REL)) {
            uint32_t target =
                seek_target(base == BUTTON_RIGHT ? 1 : -1);

            stream_seek(target, SEEK_SET);
            clock_check_time = stream_get_time();
            clock_check_tick =
                current_tick +
                CRAZYPOD_VIDEO_CLOCK_CHECK_SECONDS * HZ;
            clock_checked = paused;
            clock_stalled = false;
            draw_video_controls(video, paused,
                                base == BUTTON_RIGHT
                                    ? "FORWARD 10 SEC"
                                    : "BACK 10 SEC");
        }
        else if(base == BUTTON_SCROLL_FWD ||
                base == BUTTON_SCROLL_BACK) {
            adjust_video_volume(
                base == BUTTON_SCROLL_FWD ? 1 : -1);
            draw_video_controls(video, paused, "VOLUME");
        }
    }

cleanup:
    if(stream_opened) {
        uint32_t elapsed = stream_get_time();

        stream_stop();
        stream_close();
        if(completed ||
           (duration > 0 &&
            elapsed + CRAZYPOD_VIDEO_RESUME_END_SECONDS * TS_SECOND >=
                duration))
            video->resume_ticks = 0;
        else if(elapsed >=
                CRAZYPOD_VIDEO_RESUME_MIN_SECONDS * TS_SECOND)
            video->resume_ticks = elapsed;
        else
            video->resume_ticks = 0;
        video->duration_ticks = duration;
        save_video_state();
        mutex_lock(&video_mutex);
        ++video_generation;
        mutex_unlock(&video_mutex);
    }
    if(engine_initialized)
        stream_exit();
    video_engine_release_audio_buffer();
    cpu_boost(false);
    button_clear_queue();
    if(repost_system_event)
        button_queue_post(system_event, system_event_data);
    crazypod_videos_resume();
    last_video_result = result;
    return result;
}

enum crazypod_video_result crazypod_video_last_result(void)
{
    return last_video_result;
}

const char *crazypod_video_result_message(
    enum crazypod_video_result result)
{
    switch(result) {
    case CRAZYPOD_VIDEO_OK:
        return "";
    case CRAZYPOD_VIDEO_INVALID_FILE:
        return "Video is no longer available";
    case CRAZYPOD_VIDEO_NO_MEMORY:
        return "Not enough memory to play this video";
    case CRAZYPOD_VIDEO_UNSUPPORTED:
        return "Convert this video to MPEG-1 or MPEG-2";
    case CRAZYPOD_VIDEO_OPEN_FAILED:
        return "Could not open this video";
    case CRAZYPOD_VIDEO_ENGINE_FAILED:
    default:
        return video_engine_message[0] != '\0'
            ? video_engine_message : "Video playback failed";
    }
}

#endif
