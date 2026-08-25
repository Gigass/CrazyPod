#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "bmp.h"
#include "buflib.h"
#include "core_alloc.h"
#include "file.h"
#include "kernel.h"
#include "panic.h"

#include "../crazypod_image.h"
#include "../crazypod_videos.h"
#include "crazypod_video_catalog.h"
#include "crazypod_video_poster.h"

#define POSTER_DECODE_EXTRA (64u * 1024u)
#define POSTER_STACK_SIZE 0x3000
#define POSTER_WAKE 1

struct poster_slot {
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

static struct poster_slot slot;
static struct mutex poster_mutex;
static struct event_queue poster_queue;
static long poster_stack[POSTER_STACK_SIZE / sizeof(long)];
static bool suspended;
static bool decoding;
static bool wake_queued;
static unsigned generation;

static bool decode_poster(const char *path,
                          lv_image_dsc_t *descriptor,
                          fb_data *destination)
{
    struct bitmap bitmap;
    size_t pixel_bytes =
        (size_t)CRAZYPOD_VIDEO_POSTER_WIDTH *
        CRAZYPOD_VIDEO_POSTER_HEIGHT * sizeof(fb_data);
    size_t decode_bytes = pixel_bytes + POSTER_DECODE_EXTRA;
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

static void poster_thread(void)
{
    for(;;) {
        struct queue_event event;

        queue_wait(&poster_queue, &event);
        if(event.id != POSTER_WAKE)
            continue;
        for(;;) {
            char path[MAX_PATH];
            unsigned serial;
            int index;
            int bank;
            bool valid;

            mutex_lock(&poster_mutex);
            if(suspended || !slot.pending) {
                decoding = false;
                wake_queued = false;
                mutex_unlock(&poster_mutex);
                break;
            }
            snprintf(path, sizeof(path), "%s", slot.requested_path);
            serial = slot.request_serial;
            index = slot.requested_index;
            bank = 1 - slot.active_bank;
            slot.pending = false;
            decoding = true;
            mutex_unlock(&poster_mutex);

            valid = decode_poster(
                path, &slot.descriptor[bank], slot.pixels[bank]);

            mutex_lock(&poster_mutex);
            if(serial == slot.request_serial &&
               index == slot.requested_index &&
               strcmp(path, slot.requested_path) == 0) {
                slot.decoded_index = index;
                slot.decoded_serial = serial;
                slot.active_bank = bank;
                slot.valid = valid;
                ++generation;
            }
            decoding = false;
            mutex_unlock(&poster_mutex);
        }
    }
}

static void wake_worker(void)
{
    bool post = false;

    mutex_lock(&poster_mutex);
    if(!suspended && !wake_queued) {
        wake_queued = true;
        post = true;
    }
    mutex_unlock(&poster_mutex);
    if(post)
        queue_post(&poster_queue, POSTER_WAKE, 0);
}

void crazypod_video_poster_init(void)
{
    memset(&slot, 0, sizeof(slot));
    slot.requested_index = -1;
    slot.decoded_index = -1;
    mutex_init(&poster_mutex);
    queue_init(&poster_queue, false);
    if(create_thread(poster_thread, poster_stack, sizeof(poster_stack), 0,
                     "crazypod video posters"
                     IF_PRIO(, PRIORITY_USER_INTERFACE)
                     IF_COP(, CPU)) == 0)
        panicf("video poster thread");
}

void crazypod_video_poster_reset(void)
{
    mutex_lock(&poster_mutex);
    memset(&slot, 0, sizeof(slot));
    slot.requested_index = -1;
    slot.decoded_index = -1;
    suspended = false;
    wake_queued = false;
    ++generation;
    mutex_unlock(&poster_mutex);
}

void crazypod_video_poster_suspend(void)
{
    bool active;

    mutex_lock(&poster_mutex);
    suspended = true;
    mutex_unlock(&poster_mutex);
    do {
        mutex_lock(&poster_mutex);
        active = decoding;
        mutex_unlock(&poster_mutex);
        if(!active)
            break;
        yield();
    } while(true);
}

void crazypod_video_poster_resume(void)
{
    mutex_lock(&poster_mutex);
    suspended = false;
    wake_queued = false;
    mutex_unlock(&poster_mutex);
    wake_worker();
}

const lv_image_dsc_t *crazypod_video_poster_get(int index)
{
    const struct crazypod_video_catalog_entry *entry =
        crazypod_video_catalog_get(index);
    const lv_image_dsc_t *result = NULL;
    bool changed = false;

    if(entry == NULL)
        return NULL;
    mutex_lock(&poster_mutex);
    if(slot.decoded_index == index &&
       slot.decoded_serial == slot.request_serial &&
       slot.valid &&
       strcmp(slot.requested_path, entry->poster_path) == 0) {
        result = &slot.descriptor[slot.active_bank];
    }
    else if(slot.requested_index != index ||
            strcmp(slot.requested_path, entry->poster_path) != 0) {
        slot.requested_index = index;
        snprintf(slot.requested_path, sizeof(slot.requested_path),
                 "%s", entry->poster_path);
        ++slot.request_serial;
        slot.pending = true;
        slot.valid = false;
        changed = true;
    }
    mutex_unlock(&poster_mutex);
    if(changed)
        wake_worker();
    return result;
}

unsigned crazypod_video_poster_generation(void)
{
    unsigned value;

    mutex_lock(&poster_mutex);
    value = generation;
    mutex_unlock(&poster_mutex);
    return value;
}

bool crazypod_video_poster_busy(void)
{
    bool busy;

    mutex_lock(&poster_mutex);
    busy = decoding || (!suspended && slot.pending);
    mutex_unlock(&poster_mutex);
    return busy;
}

void crazypod_video_poster_mark_changed(void)
{
    mutex_lock(&poster_mutex);
    ++generation;
    mutex_unlock(&poster_mutex);
}

#endif
