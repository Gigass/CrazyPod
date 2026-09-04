#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "backlight.h"
#include "dir.h"
#include "file.h"
#include "general.h"
#include "kernel.h"
#include "lcd.h"
#include "mutex.h"
#include "queue.h"
#include "settings.h"
#include "system.h"

#include "platform/crazypod_platform_display.h"
#include "crazypod_screen_recording.h"

#define RECORDING_DIRECTORY "/Videos/Screen Recordings"
#define RECORDING_FPS 30u
#define RECORDING_QUEUE_SLOTS 4u
#define AVI_HEADER_SIZE 236u
#define AVI_FRAME_SIZE ((uint32_t)LCD_WIDTH * LCD_HEIGHT * 2u)
#define AVI_FRAME_BLOCK_SIZE (8u + AVI_FRAME_SIZE)
#define AVI_MAX_FILE_SIZE (1024u * 1024u * 1024u)
#define AVI_MAX_FRAMES \
    ((AVI_MAX_FILE_SIZE - AVI_HEADER_SIZE) / AVI_FRAME_BLOCK_SIZE)
#define RECORDING_WAKE 0x43505257

struct recording_slot {
    uint8_t pixels[AVI_FRAME_SIZE];
};

static struct {
    struct mutex mutex;
    struct event_queue queue;
    struct recording_slot slots[RECORDING_QUEUE_SLOTS];
    long stack[DEFAULT_STACK_SIZE / sizeof(long)];
    char path[MAX_PATH];
    unsigned head;
    unsigned tail;
    unsigned count;
    uint32_t frames_written;
    uint32_t frames_scheduled;
    uint32_t dropped_frames;
    long countdown_started_at;
    long started_at;
    long stopped_at;
    int fd;
    bool initialized;
    bool thread_started;
    bool countdown_active;
    int countdown_step;
    bool active;
    bool stop_requested;
    bool worker_busy;
    bool capture_in_progress;
    bool wake_queued;
    bool failed;
    bool completion_pending;
    bool backlight_suppressed;
} recording;

static void restore_backlight(void)
{
    bool restore;

    mutex_lock(&recording.mutex);
    restore = crazypod_screen_recording_claim_backlight_restore(
        &recording.backlight_suppressed);
    mutex_unlock(&recording.mutex);
    if(!restore)
        return;
    backlight_set_timeout(global_settings.backlight_timeout);
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    backlight_set_timeout_plugged(
        global_settings.backlight_timeout_plugged);
#endif
}

static void put_le16(uint8_t *data, unsigned offset, uint16_t value)
{
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
}

static void put_le32(uint8_t *data, unsigned offset, uint32_t value)
{
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
    data[offset + 2] = (uint8_t)(value >> 16);
    data[offset + 3] = (uint8_t)(value >> 24);
}

static void put_fourcc(uint8_t *data, unsigned offset, const char *value)
{
    memcpy(data + offset, value, 4);
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

static void build_header(uint8_t *header)
{
    memset(header, 0, AVI_HEADER_SIZE);
    put_fourcc(header, 0, "RIFF");
    put_fourcc(header, 8, "AVI ");
    put_fourcc(header, 12, "LIST");
    put_le32(header, 16, 204);
    put_fourcc(header, 20, "hdrl");
    put_fourcc(header, 24, "avih");
    put_le32(header, 28, 56);
    put_le32(header, 32, 1000000u / RECORDING_FPS);
    put_le32(header, 36, AVI_FRAME_BLOCK_SIZE * RECORDING_FPS);
    put_le32(header, 44, 0x00000800u);
    put_le32(header, 56, 1);
    put_le32(header, 60, AVI_FRAME_SIZE);
    put_le32(header, 64, LCD_WIDTH);
    put_le32(header, 68, LCD_HEIGHT);
    put_fourcc(header, 88, "LIST");
    put_le32(header, 92, 128);
    put_fourcc(header, 96, "strl");
    put_fourcc(header, 100, "strh");
    put_le32(header, 104, 56);
    put_fourcc(header, 108, "vids");
    put_fourcc(header, 112, "DIB ");
    put_le32(header, 128, 1);
    put_le32(header, 132, RECORDING_FPS);
    put_le32(header, 144, AVI_FRAME_SIZE);
    put_le32(header, 148, UINT32_MAX);
    put_le16(header, 160, LCD_WIDTH);
    put_le16(header, 162, LCD_HEIGHT);
    put_fourcc(header, 164, "strf");
    put_le32(header, 168, 52);
    put_le32(header, 172, 40);
    put_le32(header, 176, LCD_WIDTH);
    put_le32(header, 180, LCD_HEIGHT);
    put_le16(header, 184, 1);
    put_le16(header, 186, 16);
    put_le32(header, 188, 3);
    put_le32(header, 192, AVI_FRAME_SIZE);
    put_le32(header, 204, 0);
    put_le32(header, 208, 0);
    put_le32(header, 212, 0xf800u);
    put_le32(header, 216, 0x07e0u);
    put_le32(header, 220, 0x001fu);
    put_fourcc(header, 224, "LIST");
    put_fourcc(header, 232, "movi");
}

static uint32_t greatest_common_divisor(uint32_t left, uint32_t right)
{
    while(right != 0) {
        uint32_t remainder = left % right;

        left = right;
        right = remainder;
    }
    return left == 0 ? 1 : left;
}

static bool patch_header(
    int fd, uint32_t frames, uint32_t dropped_frames,
    long elapsed_ticks)
{
    uint8_t value[4];
    uint32_t frame_bytes = frames * AVI_FRAME_BLOCK_SIZE;
    uint32_t file_size = AVI_HEADER_SIZE + frame_bytes;
    uint32_t scale = 1;
    uint32_t rate = RECORDING_FPS;
    uint32_t frame_us = 1000000u / RECORDING_FPS;

    if(frames > 0 && dropped_frames > 0 && elapsed_ticks > 0) {
        uint32_t duration = (uint32_t)elapsed_ticks;
        uint32_t numerator = frames * (uint32_t)HZ;
        uint32_t divisor = greatest_common_divisor(duration, numerator);

        scale = duration / divisor;
        rate = numerator / divisor;
        frame_us = (uint32_t)(((uint64_t)duration * 1000000u) /
                              ((uint64_t)frames * HZ));
    }
#define PATCH_U32(offset, number) do { \
        put_le32(value, 0, (number)); \
        if(lseek(fd, (offset), SEEK_SET) != (offset) || \
           !write_exact(fd, value, sizeof(value))) \
            return false; \
    } while(0)
    PATCH_U32(4, file_size - 8u);
    PATCH_U32(32, frame_us);
    PATCH_U32(36, rate > 0
        ? (uint32_t)(((uint64_t)AVI_FRAME_BLOCK_SIZE * rate) / scale)
        : AVI_FRAME_BLOCK_SIZE * RECORDING_FPS);
    PATCH_U32(48, frames);
    PATCH_U32(128, scale);
    PATCH_U32(132, rate);
    PATCH_U32(140, frames);
    PATCH_U32(228, 4u + frame_bytes);
#undef PATCH_U32
    return lseek(fd, file_size, SEEK_SET) == (off_t)file_size;
}

static bool write_frame(int fd, const uint8_t *pixels)
{
    uint8_t chunk[8];

    put_fourcc(chunk, 0, "00db");
    put_le32(chunk, 4, AVI_FRAME_SIZE);
    return write_exact(fd, chunk, sizeof(chunk)) &&
        write_exact(fd, pixels, AVI_FRAME_SIZE);
}

static void finish_file(void)
{
    uint32_t frames;
    uint32_t dropped_frames;
    long elapsed;
    bool failed;
    int fd;
    char path[MAX_PATH];
    off_t valid_size;

    mutex_lock(&recording.mutex);
    frames = recording.frames_written;
    dropped_frames = recording.dropped_frames;
    elapsed = recording.stopped_at - recording.started_at;
    failed = recording.failed;
    fd = recording.fd;
    snprintf(path, sizeof(path), "%s", recording.path);
    mutex_unlock(&recording.mutex);

    valid_size = (off_t)AVI_HEADER_SIZE +
        (off_t)frames * AVI_FRAME_BLOCK_SIZE;
    if(ftruncate(fd, valid_size) < 0)
        failed = true;
    if(!patch_header(fd, frames, dropped_frames, elapsed))
        failed = true;
    if(fsync(fd) < 0)
        failed = true;
    if(close(fd) < 0)
        failed = true;
    if(frames == 0) {
        remove(path);
        failed = true;
    }

    mutex_lock(&recording.mutex);
    recording.fd = -1;
    recording.failed = failed;
    recording.worker_busy = false;
    recording.wake_queued = false;
    recording.stop_requested = false;
    recording.completion_pending = true;
    mutex_unlock(&recording.mutex);
    restore_backlight();
}

static void recording_thread(void)
{
    for(;;) {
        struct queue_event event;

        queue_wait(&recording.queue, &event);
        if(event.id != RECORDING_WAKE)
            continue;
        for(;;) {
            unsigned slot;
            bool should_finish;
            bool written = true;
            off_t valid_size;

            mutex_lock(&recording.mutex);
            if(recording.capture_in_progress) {
                mutex_unlock(&recording.mutex);
                sleep(1);
                continue;
            }
            if(recording.count == 0) {
                should_finish = recording.stop_requested;
                if(!should_finish) {
                    recording.worker_busy = false;
                    recording.wake_queued = false;
                }
                mutex_unlock(&recording.mutex);
                if(should_finish)
                    finish_file();
                break;
            }
            slot = recording.head;
            recording.worker_busy = true;
            mutex_unlock(&recording.mutex);

            written = write_frame(
                recording.fd, recording.slots[slot].pixels);

            mutex_lock(&recording.mutex);
            if(written)
                ++recording.frames_written;
            else {
                recording.failed = true;
                recording.active = false;
                recording.stop_requested = true;
                recording.stopped_at = current_tick;
            }
            recording.head =
                (recording.head + 1u) % RECORDING_QUEUE_SLOTS;
            --recording.count;
            if(!written)
                recording.count = 0;
            should_finish = recording.stop_requested &&
                recording.count == 0;
            valid_size = (off_t)AVI_HEADER_SIZE +
                (off_t)recording.frames_written * AVI_FRAME_BLOCK_SIZE;
            mutex_unlock(&recording.mutex);
            if(!written)
                (void)ftruncate(recording.fd, valid_size);
            if(should_finish) {
                finish_file();
                break;
            }
        }
    }
}

void crazypod_screen_recording_init(void)
{
    unsigned int id;

    if(recording.initialized)
        return;
    memset(&recording, 0, sizeof(recording));
    recording.fd = -1;
    mutex_init(&recording.mutex);
    queue_init(&recording.queue, false);
    id = create_thread(
        recording_thread, recording.stack, sizeof(recording.stack), 0,
        "crazypod screen recorder"
        IF_PRIO(, PRIORITY_BACKGROUND)
        IF_COP(, CPU));
    recording.thread_started = id != 0;
    recording.initialized = true;
}

static bool start_recording(long now)
{
    uint8_t header[AVI_HEADER_SIZE];
    char path[MAX_PATH];
    int fd;

    crazypod_screen_recording_init();
    if(!recording.thread_started)
        return false;
    mutex_lock(&recording.mutex);
    if(recording.fd >= 0) {
        mutex_unlock(&recording.mutex);
        return false;
    }
    mutex_unlock(&recording.mutex);
    mkdir("/Videos");
    mkdir(RECORDING_DIRECTORY);
    if(create_numbered_filename(
           path, RECORDING_DIRECTORY,
           "ScreenRecording_", ".avi", 4
           IF_CNFN_NUM_(, NULL)) == NULL)
        return false;
    fd = creat(path, 0666);
    if(fd < 0)
        return false;
    build_header(header);
    if(!write_exact(fd, header, sizeof(header))) {
        close(fd);
        remove(path);
        return false;
    }

    mutex_lock(&recording.mutex);
    snprintf(recording.path, sizeof(recording.path), "%s", path);
    recording.head = 0;
    recording.tail = 0;
    recording.count = 0;
    recording.frames_written = 0;
    recording.frames_scheduled = 0;
    recording.dropped_frames = 0;
    recording.started_at = now;
    recording.stopped_at = now;
    recording.fd = fd;
    recording.active = true;
    recording.stop_requested = false;
    recording.worker_busy = false;
    recording.wake_queued = false;
    recording.failed = false;
    recording.completion_pending = false;
    recording.backlight_suppressed = true;
    mutex_unlock(&recording.mutex);
    backlight_on();
    backlight_set_timeout(0);
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    backlight_set_timeout_plugged(0);
#endif
    return true;
}

static bool start_countdown(long now)
{
    crazypod_screen_recording_init();
    if(!recording.thread_started)
        return false;
    mutex_lock(&recording.mutex);
    if(recording.fd >= 0 || recording.active ||
       recording.countdown_active) {
        mutex_unlock(&recording.mutex);
        return false;
    }
    recording.countdown_started_at = now;
    recording.countdown_step = 3;
    recording.countdown_active = true;
    recording.failed = false;
    recording.completion_pending = false;
    mutex_unlock(&recording.mutex);
    backlight_on();
    return true;
}

bool crazypod_screen_recording_stop(long now)
{
    bool finished;
    bool failed;

    crazypod_screen_recording_init();
    mutex_lock(&recording.mutex);
    if(recording.countdown_active) {
        recording.countdown_active = false;
        recording.countdown_step = 0;
        mutex_unlock(&recording.mutex);
        return true;
    }
    if(!recording.active) {
        if(recording.fd >= 0) {
            recording.stop_requested = true;
            recording.stopped_at = now;
            if(!recording.wake_queued) {
                recording.wake_queued = true;
                queue_post(&recording.queue, RECORDING_WAKE, 0);
            }
            mutex_unlock(&recording.mutex);
        }
        else {
            failed = recording.failed;
            mutex_unlock(&recording.mutex);
            restore_backlight();
            return !failed;
        }
    }
    else {
        recording.active = false;
        recording.stop_requested = true;
        recording.stopped_at = now;
        if(!recording.wake_queued) {
            recording.wake_queued = true;
            queue_post(&recording.queue, RECORDING_WAKE, 0);
        }
        mutex_unlock(&recording.mutex);
    }

    do {
        mutex_lock(&recording.mutex);
        finished = recording.fd < 0;
        failed = recording.failed;
        mutex_unlock(&recording.mutex);
        if(!finished)
            yield();
    } while(!finished);
    mutex_lock(&recording.mutex);
    recording.completion_pending = false;
    mutex_unlock(&recording.mutex);
    restore_backlight();
    return !failed;
}

enum crazypod_screen_recording_result
crazypod_screen_recording_toggle(long now)
{
    bool countdown_active;

    crazypod_screen_recording_init();
    if(crazypod_screen_recording_active())
        return crazypod_screen_recording_stop(now)
            ? CRAZYPOD_SCREEN_RECORDING_STOPPED
            : CRAZYPOD_SCREEN_RECORDING_FAILED;
    mutex_lock(&recording.mutex);
    countdown_active = recording.countdown_active;
    mutex_unlock(&recording.mutex);
    if(countdown_active)
        return crazypod_screen_recording_stop(now)
            ? CRAZYPOD_SCREEN_RECORDING_CANCELLED
            : CRAZYPOD_SCREEN_RECORDING_FAILED;
    return start_countdown(now)
        ? CRAZYPOD_SCREEN_RECORDING_COUNTDOWN_STARTED
        : CRAZYPOD_SCREEN_RECORDING_FAILED;
}

static void copy_frame(uint8_t *destination)
{
    const fb_data *framebuffer =
        crazypod_platform_display_framebuffer();
    int row;

    for(row = LCD_HEIGHT - 1; row >= 0; --row) {
        const fb_data *source = framebuffer + row * LCD_WIDTH;
        int column;

        for(column = 0; column < LCD_WIDTH; ++column) {
#if LCD_PIXELFORMAT == RGB565SWAPPED
            uint16_t pixel = htobe16(source[column]);
#else
            uint16_t pixel = htole16(source[column]);
#endif
            *destination++ = (uint8_t)pixel;
            *destination++ = (uint8_t)(pixel >> 8);
        }
    }
}

enum crazypod_screen_recording_event
crazypod_screen_recording_service(long now)
{
    bool post = false;
    bool active;
    bool countdown_active;
    bool completion;
    bool failed;
    int countdown_step;
    long countdown_started_at;
    uint32_t expected_frames;
    unsigned slot;

    mutex_lock(&recording.mutex);
    active = recording.active;
    countdown_active = recording.countdown_active;
    countdown_step = recording.countdown_step;
    countdown_started_at = recording.countdown_started_at;
    completion = recording.completion_pending;
    failed = recording.failed;
    if(completion)
        recording.completion_pending = false;
    mutex_unlock(&recording.mutex);
    if(completion) {
        restore_backlight();
        return crazypod_screen_recording_completion_event(failed);
    }
    if(countdown_active) {
        long elapsed = now - countdown_started_at;

        if(elapsed >= 3 * HZ) {
            mutex_lock(&recording.mutex);
            recording.countdown_active = false;
            recording.countdown_step = 0;
            mutex_unlock(&recording.mutex);
            return start_recording(now)
                ? CRAZYPOD_SCREEN_RECORDING_EVENT_STARTED
                : CRAZYPOD_SCREEN_RECORDING_EVENT_FAILED;
        }
        if(countdown_step == 3 && elapsed >= HZ) {
            mutex_lock(&recording.mutex);
            recording.countdown_step = 2;
            mutex_unlock(&recording.mutex);
            return CRAZYPOD_SCREEN_RECORDING_EVENT_COUNTDOWN_2;
        }
        if(countdown_step == 2 && elapsed >= 2 * HZ) {
            mutex_lock(&recording.mutex);
            recording.countdown_step = 1;
            mutex_unlock(&recording.mutex);
            return CRAZYPOD_SCREEN_RECORDING_EVENT_COUNTDOWN_1;
        }
        return CRAZYPOD_SCREEN_RECORDING_EVENT_NONE;
    }
    if(!active)
        return CRAZYPOD_SCREEN_RECORDING_EVENT_NONE;

    expected_frames =
        (uint32_t)(((uint64_t)(now - recording.started_at) *
                    RECORDING_FPS) / HZ);
    mutex_lock(&recording.mutex);
    if(recording.frames_written + recording.count >= AVI_MAX_FRAMES) {
        mutex_unlock(&recording.mutex);
        return crazypod_screen_recording_limit_event(
            crazypod_screen_recording_stop(now));
    }
    if(recording.frames_scheduled >= expected_frames) {
        mutex_unlock(&recording.mutex);
        return CRAZYPOD_SCREEN_RECORDING_EVENT_NONE;
    }
    ++recording.frames_scheduled;
    if(recording.count >= RECORDING_QUEUE_SLOTS ||
       recording.capture_in_progress) {
        ++recording.dropped_frames;
        mutex_unlock(&recording.mutex);
        return CRAZYPOD_SCREEN_RECORDING_EVENT_NONE;
    }
    slot = recording.tail;
    recording.tail =
        (recording.tail + 1u) % RECORDING_QUEUE_SLOTS;
    ++recording.count;
    recording.capture_in_progress = true;
    if(!recording.wake_queued) {
        recording.wake_queued = true;
        post = true;
    }
    mutex_unlock(&recording.mutex);
    /* Frame conversion is deliberately outside the control mutex.  A full
     * LCD copy can take long enough on the iPod to make wheel input feel
     * stuck; the worker waits on capture_in_progress before consuming the
     * reserved slot. */
    copy_frame(recording.slots[slot].pixels);
    mutex_lock(&recording.mutex);
    recording.capture_in_progress = false;
    mutex_unlock(&recording.mutex);
    if(post)
        queue_post(&recording.queue, RECORDING_WAKE, 0);
    return CRAZYPOD_SCREEN_RECORDING_EVENT_NONE;
}

int crazypod_screen_recording_wait_ticks(long now)
{
    uint32_t captured;
    long due;
    int wait;

    mutex_lock(&recording.mutex);
    if(recording.countdown_active) {
        due = recording.countdown_started_at +
            (long)(4 - recording.countdown_step) * HZ;
        mutex_unlock(&recording.mutex);
        wait = (int)(due - now);
        return wait > 0 ? wait : 1;
    }
    if(!recording.active) {
        mutex_unlock(&recording.mutex);
        return HZ;
    }
    captured = recording.frames_scheduled;
    due = recording.started_at +
        (long)(((uint64_t)(captured + 1u) * HZ +
                RECORDING_FPS - 1u) /
               RECORDING_FPS);
    mutex_unlock(&recording.mutex);
    wait = (int)(due - now);
    return wait > 0 ? wait : 1;
}

bool crazypod_screen_recording_active(void)
{
    bool active;

    if(!recording.initialized)
        return false;
    mutex_lock(&recording.mutex);
    active = recording.active;
    mutex_unlock(&recording.mutex);
    return active;
}

uint32_t crazypod_screen_recording_dropped_frames(void)
{
    uint32_t dropped;

    if(!recording.initialized)
        return 0;
    mutex_lock(&recording.mutex);
    dropped = recording.dropped_frames;
    mutex_unlock(&recording.mutex);
    return dropped;
}

#endif
