#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bmp.h"
#include "buflib.h"
#include "core_alloc.h"
#include "dir.h"
#include "file.h"
#include "jpeg_load.h"
#include "kernel.h"
#include "lcd.h"
#include "panic.h"
#include "resize.h"
#include "string-extra.h"
#include "system.h"

#include "src/misc/cache/instance/lv_image_cache.h"

#include "crazypod_image.h"
#include "crazypod_photos.h"
#include "photos/crazypod_photo_cache.h"
#include "photos/crazypod_photo_catalog.h"
#include "photos/crazypod_photo_viewport.h"

#define CRAZYPOD_PHOTO_THREAD_STACK_SIZE 0x4000
#define CRAZYPOD_PHOTO_WAKE 1
#define CRAZYPOD_PHOTO_REFRESH 2
#define CRAZYPOD_PHOTO_SAVE 3
#define CRAZYPOD_PHOTO_SAVE_DEBOUNCE (HZ / 2)
#define CRAZYPOD_PHOTO_DECODE_EXTRA (64 * 1024)
struct photo_slot {
    fb_data pixels[2][CRAZYPOD_PHOTO_THUMB_SIZE *
                      CRAZYPOD_PHOTO_THUMB_SIZE]
        CACHEALIGN_AT_LEAST_ATTR(16);
    lv_image_dsc_t descriptor[2];
    char requested_path[MAX_PATH];
    uint32_t requested_size;
    uint32_t requested_mtime;
    unsigned request_serial;
    unsigned decoded_serial;
    int requested_index;
    int decoded_index;
    int active_bank;
    bool pending;
    bool decoding;
    bool valid;
};

struct photo_view_slot {
    fb_data pixels[2][CRAZYPOD_PHOTO_VIEW_WIDTH *
                      CRAZYPOD_PHOTO_VIEW_HEIGHT]
        CACHEALIGN_AT_LEAST_ATTR(16);
    lv_image_dsc_t descriptor[2];
    char requested_path[MAX_PATH];
    uint32_t requested_size;
    uint32_t requested_mtime;
    unsigned request_serial;
    unsigned decoded_serial;
    int requested_index;
    int decoded_index;
    int active_bank;
    int progress;
    bool pending;
    bool valid;
};

struct photo_decode_request {
    char path[MAX_PATH];
    uint32_t size;
    uint32_t mtime;
    unsigned serial;
    int index;
    int slot;
    int width;
    int height;
    bool view;
};


static struct photo_slot thumbnail_slots[CRAZYPOD_PHOTO_THUMB_SLOTS];
static struct photo_view_slot view_slot;
static unsigned photo_publish_generation;
static unsigned photo_thumbnail_publish_generation;
static unsigned photo_view_publish_generation;
static bool photo_worker_decoding;
static bool photo_suspended;
static bool photo_storage_suspended;
static bool photo_lock_suspended;
static bool photo_route_suspended;
static bool photo_refresh_pending;
static bool photo_refresh_queued;
static bool photo_catalog_refreshing;
static bool photo_save_queued;
static bool photo_cache_initialized;
static bool photo_wake_queued;
static struct mutex photo_mutex;
static struct event_queue photo_queue;
static long photo_stack[CRAZYPOD_PHOTO_THREAD_STACK_SIZE /
                        sizeof(long)];

static void wake_worker(void);

static int begin_background_work(void)
{
#ifdef HAVE_PRIORITY_SCHEDULING
    return thread_set_priority(
        thread_self(), PRIORITY_BACKGROUND);
#else
    return -1;
#endif
}

static void finish_background_work(int old_priority)
{
#ifdef HAVE_PRIORITY_SCHEDULING
    if(old_priority >= HIGHEST_PRIORITY &&
       old_priority <= LOWEST_PRIORITY)
        thread_set_priority(thread_self(), old_priority);
#else
    (void)old_priority;
#endif
}

 static bool decode_photo(const struct photo_decode_request *request,
                         lv_image_dsc_t *descriptor,
                         fb_data *destination)
{
    struct bitmap bitmap;
    fb_data *decode_buffer;
    size_t pixel_bytes =
        (size_t)request->width * request->height * sizeof(fb_data);
    size_t decode_bytes = pixel_bytes + CRAZYPOD_PHOTO_DECODE_EXTRA;
    int decode_handle;
    int result;
    int row;

    decode_handle = core_alloc_ex(decode_bytes, &buflib_ops_locked);
    if(decode_handle < 0)
        return false;
    decode_buffer = core_get_data(decode_handle);
    memset(&bitmap, 0, sizeof(bitmap));
    bitmap.width = request->width;
    bitmap.height = request->height;
    bitmap.data = (unsigned char *)decode_buffer;
    crazypod_image_decode_lock();
    if(crazypod_photo_catalog_path_supported(request->path) &&
       (strcasecmp(strrchr(request->path, '.'), ".jpg") == 0 ||
        strcasecmp(strrchr(request->path, '.'), ".jpeg") == 0)) {
        result = read_jpeg_file(
            request->path, &bitmap, decode_bytes,
            FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT,
            &format_native);
    }
    else {
        result = read_bmp_file(
            request->path, &bitmap, decode_bytes,
            FORMAT_NATIVE | FORMAT_RESIZE | FORMAT_KEEP_ASPECT,
            &format_native);
    }
    crazypod_image_decode_unlock();
    if(result < 0 || bitmap.width <= 0 || bitmap.height <= 0 ||
       bitmap.width > request->width ||
       bitmap.height > request->height || bitmap.data == NULL) {
        core_free(decode_handle);
        return false;
    }
    for(row = 0; row < bitmap.height; ++row) {
        memcpy(destination + row * bitmap.width,
               (fb_data *)bitmap.data + row * bitmap.width,
               (size_t)bitmap.width * sizeof(fb_data));
    }
    core_free(decode_handle);
    return crazypod_image_configure_rgb565(
        descriptor, destination, bitmap.width, bitmap.height);
}

static bool find_request(struct photo_decode_request *request)
{
    int slot;

    memset(request, 0, sizeof(*request));
    mutex_lock(&photo_mutex);
    if(photo_suspended) {
        photo_wake_queued = false;
        mutex_unlock(&photo_mutex);
        return false;
    }
    if(view_slot.pending) {
        request->view = true;
        request->index = view_slot.requested_index;
        request->serial = view_slot.request_serial;
        request->size = view_slot.requested_size;
        request->mtime = view_slot.requested_mtime;
        request->width = CRAZYPOD_PHOTO_VIEW_WIDTH;
        request->height = CRAZYPOD_PHOTO_VIEW_HEIGHT;
        snprintf(request->path, sizeof(request->path), "%s",
                 view_slot.requested_path);
        view_slot.pending = false;
        view_slot.progress = 25;
        photo_worker_decoding = true;
        mutex_unlock(&photo_mutex);
        return true;
    }
    for(slot = 0; slot < CRAZYPOD_PHOTO_THUMB_SLOTS; ++slot) {
        if(!thumbnail_slots[slot].pending ||
           thumbnail_slots[slot].decoding)
            continue;
        request->slot = slot;
        request->index = thumbnail_slots[slot].requested_index;
        request->serial = thumbnail_slots[slot].request_serial;
        request->size = thumbnail_slots[slot].requested_size;
        request->mtime = thumbnail_slots[slot].requested_mtime;
        request->width = CRAZYPOD_PHOTO_THUMB_SIZE;
        request->height = CRAZYPOD_PHOTO_THUMB_SIZE;
        snprintf(request->path, sizeof(request->path), "%s",
                 thumbnail_slots[slot].requested_path);
        thumbnail_slots[slot].pending = false;
        thumbnail_slots[slot].decoding = true;
        photo_worker_decoding = true;
        mutex_unlock(&photo_mutex);
        return true;
    }
    photo_worker_decoding = false;
    photo_wake_queued = false;
    mutex_unlock(&photo_mutex);
    return false;
}

static void complete_request(const struct photo_decode_request *request,
                             int bank, bool valid)
{
    mutex_lock(&photo_mutex);
    if(request->view) {
        if(view_slot.request_serial == request->serial &&
           view_slot.requested_size == request->size &&
           view_slot.requested_mtime == request->mtime &&
           strcmp(view_slot.requested_path, request->path) == 0) {
            view_slot.active_bank = bank;
            view_slot.decoded_index = request->index;
            view_slot.decoded_serial = request->serial;
            view_slot.valid = valid;
            view_slot.progress = valid ? 100 : -1;
            ++photo_view_publish_generation;
        }
    }
    else if(request->slot >= 0 &&
            request->slot < CRAZYPOD_PHOTO_THUMB_SLOTS) {
        struct photo_slot *slot = &thumbnail_slots[request->slot];

        if(slot->request_serial == request->serial &&
           slot->requested_size == request->size &&
           slot->requested_mtime == request->mtime &&
           strcmp(slot->requested_path, request->path) == 0) {
            slot->active_bank = bank;
            slot->decoded_index = request->index;
            slot->decoded_serial = request->serial;
            slot->valid = valid;
            ++photo_thumbnail_publish_generation;
        }
        slot->decoding = false;
    }
    photo_worker_decoding = false;
    mutex_unlock(&photo_mutex);
}

static void update_view_progress(
    const struct photo_decode_request *request, int progress)
{
    if(request == NULL || !request->view)
        return;
    mutex_lock(&photo_mutex);
    if(view_slot.request_serial == request->serial &&
       view_slot.requested_index == request->index &&
       strcmp(view_slot.requested_path, request->path) == 0)
        view_slot.progress = progress;
    mutex_unlock(&photo_mutex);
}

static void ensure_photo_cache_initialized(void)
{
    bool initialize;

    mutex_lock(&photo_mutex);
    initialize = !photo_cache_initialized;
    if(initialize)
        photo_cache_initialized = true;
    mutex_unlock(&photo_mutex);
    if(initialize)
        crazypod_photo_cache_init();
}

static void photo_thread(void)
{
    struct queue_event event;

    while(true) {
        queue_wait(&photo_queue, &event);
        if(event.id == CRAZYPOD_PHOTO_REFRESH) {
            int old_priority;
            bool wake;
            bool cancelled;

            mutex_lock(&photo_mutex);
            photo_refresh_queued = false;
            cancelled = photo_storage_suspended ||
                photo_lock_suspended || photo_route_suspended;
            mutex_unlock(&photo_mutex);
            if(!cancelled) {
                old_priority = begin_background_work();
                crazypod_photo_catalog_refresh();
                finish_background_work(old_priority);
            }
            mutex_lock(&photo_mutex);
            photo_catalog_refreshing = false;
            photo_suspended =
                photo_storage_suspended || photo_lock_suspended ||
                photo_route_suspended;
            if(cancelled || photo_suspended)
                photo_refresh_pending = true;
            photo_wake_queued = false;
            ++photo_publish_generation;
            ++photo_view_publish_generation;
            crazypod_photo_viewport_reset();
            wake = !photo_suspended;
            mutex_unlock(&photo_mutex);
            if(wake)
                wake_worker();
            continue;
        }
        if(event.id == CRAZYPOD_PHOTO_SAVE) {
            bool save;

            sleep(CRAZYPOD_PHOTO_SAVE_DEBOUNCE > 0
                      ? CRAZYPOD_PHOTO_SAVE_DEBOUNCE : 1);
            mutex_lock(&photo_mutex);
            save = photo_save_queued;
            photo_save_queued = false;
            mutex_unlock(&photo_mutex);
            if(save)
                (void)crazypod_photo_catalog_save();
            continue;
        }
        if(event.id != CRAZYPOD_PHOTO_WAKE)
            continue;
        while(true) {
            struct photo_decode_request request;
            lv_image_dsc_t *descriptor;
            fb_data *pixels;
            int bank;
            bool cache_hit = false;
            bool valid;

            if(!find_request(&request))
                break;
            ensure_photo_cache_initialized();
            if(request.view) {
                bank = 1 - view_slot.active_bank;
                descriptor = &view_slot.descriptor[bank];
                pixels = view_slot.pixels[bank];
            }
            else {
                struct photo_slot *slot =
                    &thumbnail_slots[request.slot];

                bank = 1 - slot->active_bank;
                descriptor = &slot->descriptor[bank];
                pixels = slot->pixels[bank];
            }
            if(request.view)
                update_view_progress(&request, 35);
            if(request.view)
                cache_hit = crazypod_photo_cache_load(
                    true, request.path, request.size, request.mtime,
                    descriptor, pixels);
            else
                cache_hit = crazypod_photo_cache_load(
                    false, request.path, request.size, request.mtime,
                    descriptor, pixels);
            if(request.view && !cache_hit)
                update_view_progress(&request, 45);
            valid = cache_hit ||
                decode_photo(&request, descriptor, pixels);
            if(request.view && valid)
                update_view_progress(&request, 90);
            if(valid && request.view && !cache_hit)
                crazypod_photo_cache_store(
                    true, request.path, request.size, request.mtime,
                    descriptor);
            else if(valid && !request.view && !cache_hit)
                crazypod_photo_cache_store(
                    false, request.path, request.size, request.mtime,
                    descriptor);
            complete_request(&request, bank, valid);
            yield();
        }
    }
}

static void wake_worker(void)
{
    bool post = false;

    mutex_lock(&photo_mutex);
    if(!photo_suspended && !photo_wake_queued) {
        photo_wake_queued = true;
        post = true;
    }
    mutex_unlock(&photo_mutex);
    if(post)
        queue_post(&photo_queue, CRAZYPOD_PHOTO_WAKE, 0);
}

static void schedule_catalog_save(void)
{
    bool post = false;

    mutex_lock(&photo_mutex);
    if(!photo_save_queued) {
        photo_save_queued = true;
        post = true;
    }
    mutex_unlock(&photo_mutex);
    if(post)
        queue_post(&photo_queue, CRAZYPOD_PHOTO_SAVE, 0);
}

static void wait_for_photo_idle(void)
{
    bool active;

    do {
        mutex_lock(&photo_mutex);
        active = photo_worker_decoding;
        mutex_unlock(&photo_mutex);
        if(active)
            yield();
    } while(active);
}

static void wait_for_photo_catalog_idle(void)
{
    bool active;

    do {
        mutex_lock(&photo_mutex);
        active = photo_catalog_refreshing || photo_refresh_queued;
        mutex_unlock(&photo_mutex);
        if(active)
            yield();
    } while(active);
}

void crazypod_photos_init(void)
{
    bool catalog_loaded;
    int slot;

    memset(thumbnail_slots, 0, sizeof(thumbnail_slots));
    memset(&view_slot, 0, sizeof(view_slot));
    for(slot = 0; slot < CRAZYPOD_PHOTO_THUMB_SLOTS; ++slot) {
        thumbnail_slots[slot].requested_index = -1;
        thumbnail_slots[slot].decoded_index = -1;
    }
    view_slot.requested_index = -1;
    view_slot.decoded_index = -1;
    photo_worker_decoding = false;
    photo_suspended = true;
    photo_storage_suspended = false;
    photo_lock_suspended = false;
    photo_route_suspended = true;
    photo_refresh_pending = false;
    photo_refresh_queued = false;
    photo_catalog_refreshing = false;
    photo_save_queued = false;
    photo_cache_initialized = false;
    photo_wake_queued = false;
    mutex_init(&photo_mutex);
    queue_init(&photo_queue, false);
    if(create_thread(photo_thread, photo_stack, sizeof(photo_stack), 0,
                     "crazypod photos"
                     IF_PRIO(, PRIORITY_BACKGROUND)
                     IF_COP(, CPU)) == 0)
        panicf("photo thread");
    catalog_loaded = crazypod_photo_catalog_init();
    photo_refresh_pending = !catalog_loaded;
}

void crazypod_photos_refresh(void)
{
    bool post = false;

    mutex_lock(&photo_mutex);
    if(photo_storage_suspended || photo_lock_suspended ||
       photo_route_suspended) {
        photo_refresh_pending = true;
        mutex_unlock(&photo_mutex);
        return;
    }
    if(photo_catalog_refreshing || photo_refresh_queued) {
        mutex_unlock(&photo_mutex);
        return;
    }
    photo_refresh_pending = false;
    photo_suspended = true;
    photo_catalog_refreshing = true;
    photo_refresh_queued = true;
    post = true;
    mutex_unlock(&photo_mutex);
    if(post) {
        crazypod_photo_catalog_reset_refresh_cancel();
        queue_post(&photo_queue, CRAZYPOD_PHOTO_REFRESH, 0);
    }
}

void crazypod_photos_ensure_catalog(void)
{
    bool refresh;

    mutex_lock(&photo_mutex);
    refresh = !photo_storage_suspended && !photo_lock_suspended &&
        !photo_route_suspended && photo_refresh_pending;
    mutex_unlock(&photo_mutex);
    if(refresh)
        crazypod_photos_refresh();
}

void crazypod_photos_suspend(void)
{
    mutex_lock(&photo_mutex);
    photo_storage_suspended = true;
    photo_suspended = true;
    mutex_unlock(&photo_mutex);
    crazypod_photo_catalog_cancel_refresh();
    wait_for_photo_idle();
    wait_for_photo_catalog_idle();
    crazypod_photo_catalog_reset_refresh_cancel();
    mutex_lock(&photo_mutex);
    photo_save_queued = false;
    mutex_unlock(&photo_mutex);
    (void)crazypod_photo_catalog_save();
}

void crazypod_photos_resume(void)
{
    mutex_lock(&photo_mutex);
    photo_storage_suspended = false;
    photo_suspended =
        photo_lock_suspended || photo_route_suspended;
    photo_refresh_pending = true;
    photo_wake_queued = false;
    mutex_unlock(&photo_mutex);
}

void crazypod_photos_set_lock_suspended(bool suspended)
{
    bool wake = false;

    mutex_lock(&photo_mutex);
    if(photo_lock_suspended != suspended) {
        photo_lock_suspended = suspended;
        photo_suspended =
            photo_storage_suspended || photo_lock_suspended ||
            photo_route_suspended;
        if(!photo_suspended && !photo_refresh_pending &&
           !photo_catalog_refreshing)
            wake = true;
    }
    mutex_unlock(&photo_mutex);
    if(wake)
        wake_worker();
}

void crazypod_photos_set_route_suspended(bool suspended)
{
    bool wake = false;

    mutex_lock(&photo_mutex);
    if(photo_route_suspended != suspended) {
        photo_route_suspended = suspended;
        photo_suspended =
            photo_storage_suspended || photo_lock_suspended ||
            photo_route_suspended;
        if(!photo_suspended && !photo_refresh_pending &&
           !photo_catalog_refreshing)
            wake = true;
    }
    mutex_unlock(&photo_mutex);
    if(wake)
        wake_worker();
}

void crazypod_photos_invalidate_catalog(void)
{
    mutex_lock(&photo_mutex);
    photo_refresh_pending = true;
    mutex_unlock(&photo_mutex);
    crazypod_photo_catalog_invalidate();
    /* USB storage can change only part of /Pictures. Cache entries already
       include the source path key, size and mtime, so changed files miss
       naturally while unchanged thumbnails and views remain reusable. */
}

void crazypod_photos_note_file_added(void)
{
    mutex_lock(&photo_mutex);
    photo_refresh_pending = true;
    mutex_unlock(&photo_mutex);
    crazypod_photo_catalog_invalidate();
}

int crazypod_photo_count(void)
{
    return crazypod_photo_catalog_count();
}

int crazypod_photo_favorite_count(void)
{
    return crazypod_photo_catalog_favorite_count();
}

int crazypod_photo_favorite_index(int favorite_index)
{
    return crazypod_photo_catalog_favorite_index(favorite_index);
}

const char *crazypod_photo_path(int index)
{
    static char path[MAX_PATH];
    struct crazypod_photo_catalog_entry entry;

    if(!crazypod_photo_catalog_copy(index, &entry)) {
        path[0] = '\0';
        return path;
    }
    snprintf(path, sizeof(path), "%s", entry.path);
    return path;
}

const char *crazypod_photo_name(int index)
{
    return crazypod_photo_catalog_name(index);
}

bool crazypod_photo_is_favorite(int index)
{
    struct crazypod_photo_catalog_entry entry;

    return crazypod_photo_catalog_copy(index, &entry) && entry.favorite;
}

bool crazypod_photo_toggle_favorite(int index)
{
    if(!crazypod_photo_catalog_toggle_favorite(index))
        return false;
    schedule_catalog_save();
    ++photo_publish_generation;
    return true;
}

bool crazypod_photo_delete(int index)
{
    bool deleted;
    bool wake;
    char deleted_path[MAX_PATH];
    int slot;

    snprintf(deleted_path, sizeof(deleted_path), "%s",
             crazypod_photo_path(index));
    if(deleted_path[0] == '\0')
        return false;

    mutex_lock(&photo_mutex);
    if(photo_storage_suspended || photo_lock_suspended) {
        mutex_unlock(&photo_mutex);
        return false;
    }
    photo_suspended = true;
    mutex_unlock(&photo_mutex);
    wait_for_photo_idle();
    /* A catalog refresh owns the catalog I/O lock while it walks the
     * filesystem. Cancel and drain it before deleting so the wheel handler
     * never waits behind a full media scan. */
    crazypod_photo_catalog_cancel_refresh();
    wait_for_photo_catalog_idle();
    crazypod_photo_catalog_reset_refresh_cancel();
    deleted = crazypod_photo_catalog_delete(index);
    if(deleted)
        schedule_catalog_save();
    mutex_lock(&photo_mutex);
    if(deleted) {
        for(slot = 0; slot < CRAZYPOD_PHOTO_THUMB_SLOTS; ++slot) {
            if(strcmp(thumbnail_slots[slot].requested_path,
                      deleted_path) != 0)
                continue;
            thumbnail_slots[slot].requested_index = -1;
            thumbnail_slots[slot].decoded_index = -1;
            thumbnail_slots[slot].pending = false;
            thumbnail_slots[slot].valid = false;
            ++thumbnail_slots[slot].request_serial;
        }
        if(strcmp(view_slot.requested_path, deleted_path) == 0) {
            view_slot.requested_index = -1;
            view_slot.decoded_index = -1;
            view_slot.pending = false;
            view_slot.valid = false;
            ++view_slot.request_serial;
        }
        photo_refresh_pending = false;
        ++photo_publish_generation;
        ++photo_view_publish_generation;
        crazypod_photo_viewport_reset();
    }
    photo_suspended =
        photo_storage_suspended || photo_lock_suspended ||
        photo_route_suspended;
    photo_wake_queued = false;
    wake = !photo_suspended;
    mutex_unlock(&photo_mutex);
    if(wake)
        wake_worker();
    return deleted;
}

const lv_image_dsc_t *crazypod_photo_thumbnail(int slot_index, int index)
{
    struct crazypod_photo_catalog_entry entry;
    struct photo_slot *slot = NULL;
    const lv_image_dsc_t *result = NULL;
    bool changed = false;
    int candidate;

    if(slot_index < 0 || slot_index >= CRAZYPOD_PHOTO_THUMB_SLOTS ||
       !crazypod_photo_catalog_copy(index, &entry))
        return NULL;
    mutex_lock(&photo_mutex);
    for(candidate = 0; candidate < CRAZYPOD_PHOTO_THUMB_SLOTS;
        ++candidate) {
        struct photo_slot *cached = &thumbnail_slots[candidate];

        if(cached->requested_size != entry.size ||
           cached->requested_mtime != entry.mtime ||
           strcmp(cached->requested_path, entry.path) != 0)
            continue;
        slot = cached;
        slot->requested_index = index;
        if(slot->decoded_serial == slot->request_serial && slot->valid) {
            slot->decoded_index = index;
            result = &slot->descriptor[slot->active_bank];
        }
        break;
    }
    if(slot == NULL) {
        for(candidate = 0; candidate < CRAZYPOD_PHOTO_THUMB_SLOTS;
            ++candidate) {
            if(!thumbnail_slots[candidate].valid &&
               !thumbnail_slots[candidate].pending &&
               !thumbnail_slots[candidate].decoding) {
                slot = &thumbnail_slots[candidate];
                break;
            }
        }
        if(slot == NULL && !thumbnail_slots[slot_index].decoding)
            slot = &thumbnail_slots[slot_index];
        if(slot == NULL) {
            for(candidate = 0;
                candidate < CRAZYPOD_PHOTO_THUMB_SLOTS;
                ++candidate) {
                if(!thumbnail_slots[candidate].pending &&
                   !thumbnail_slots[candidate].decoding) {
                    slot = &thumbnail_slots[candidate];
                    break;
                }
            }
        }
        if(slot == NULL) {
            mutex_unlock(&photo_mutex);
            return NULL;
        }
        slot->requested_index = index;
        slot->requested_size = entry.size;
        slot->requested_mtime = entry.mtime;
        snprintf(slot->requested_path, sizeof(slot->requested_path),
                 "%s", entry.path);
        ++slot->request_serial;
        slot->pending = true;
        slot->valid = false;
        changed = true;
    }
    mutex_unlock(&photo_mutex);
    if(changed)
        wake_worker();
    return result;
}

const lv_image_dsc_t *crazypod_photo_view(int index)
{
    struct crazypod_photo_catalog_entry entry;
    const lv_image_dsc_t *result = NULL;
    bool changed = false;

    if(!crazypod_photo_catalog_copy(index, &entry))
        return NULL;
    mutex_lock(&photo_mutex);
    if(view_slot.decoded_serial == view_slot.request_serial &&
       view_slot.valid &&
       view_slot.requested_size == entry.size &&
       view_slot.requested_mtime == entry.mtime &&
       strcmp(view_slot.requested_path, entry.path) == 0) {
        view_slot.requested_index = index;
        view_slot.decoded_index = index;
        result = &view_slot.descriptor[view_slot.active_bank];
    }
    else if(view_slot.requested_index != index ||
            view_slot.requested_size != entry.size ||
            view_slot.requested_mtime != entry.mtime ||
            strcmp(view_slot.requested_path, entry.path) != 0) {
        view_slot.requested_index = index;
        view_slot.requested_size = entry.size;
        view_slot.requested_mtime = entry.mtime;
        snprintf(view_slot.requested_path,
                 sizeof(view_slot.requested_path), "%s",
                 entry.path);
        ++view_slot.request_serial;
        view_slot.pending = true;
        view_slot.valid = false;
        view_slot.progress = 10;
        changed = true;
    }
    mutex_unlock(&photo_mutex);
    if(changed)
        wake_worker();
    return result;
}

int crazypod_photo_view_progress(int index)
{
    struct crazypod_photo_catalog_entry entry;
    int progress;

    if(!crazypod_photo_catalog_copy(index, &entry))
        return -1;
    mutex_lock(&photo_mutex);
    if(view_slot.requested_index != index ||
       strcmp(view_slot.requested_path, entry.path) != 0)
        progress = 0;
    else
        progress = view_slot.progress;
    mutex_unlock(&photo_mutex);
    return progress;
}

static const lv_image_dsc_t *ready_thumbnail(int index)
{
    struct crazypod_photo_catalog_entry entry;
    const lv_image_dsc_t *result = NULL;
    int slot_index;

    if(!crazypod_photo_catalog_copy(index, &entry))
        return NULL;
    mutex_lock(&photo_mutex);
    for(slot_index = 0;
        slot_index < CRAZYPOD_PHOTO_THUMB_SLOTS; ++slot_index) {
        struct photo_slot *slot = &thumbnail_slots[slot_index];

        if(slot->decoded_serial == slot->request_serial && slot->valid &&
           slot->requested_size == entry.size &&
           slot->requested_mtime == entry.mtime &&
           strcmp(slot->requested_path, entry.path) == 0) {
            slot->requested_index = index;
            slot->decoded_index = index;
            result = &slot->descriptor[slot->active_bank];
            break;
        }
    }
    mutex_unlock(&photo_mutex);
    return result;
}

const lv_image_dsc_t *crazypod_photo_render_viewport(
    int index, int zoom_percent, int *pan_x, int *pan_y)
{
    const lv_image_dsc_t *source = crazypod_photo_view(index);

    if(source == NULL)
        source = ready_thumbnail(index);
    return crazypod_photo_viewport_render(
        index, source, zoom_percent, pan_x, pan_y);
}

const lv_image_dsc_t *crazypod_photo_render_crop_preview(
    int index, int center_x, int center_y)
{
    return crazypod_photo_viewport_render_crop(
        crazypod_photo_view(index), center_x, center_y);
}

unsigned crazypod_photo_generation(void)
{
    unsigned generation;

    mutex_lock(&photo_mutex);
    generation = photo_publish_generation;
    mutex_unlock(&photo_mutex);
    return generation;
}

unsigned crazypod_photo_thumbnail_generation(void)
{
    unsigned generation;

    mutex_lock(&photo_mutex);
    generation = photo_thumbnail_publish_generation;
    mutex_unlock(&photo_mutex);
    return generation;
}

unsigned crazypod_photo_view_generation(void)
{
    unsigned generation;

    mutex_lock(&photo_mutex);
    generation = photo_view_publish_generation;
    mutex_unlock(&photo_mutex);
    return generation;
}

bool crazypod_photos_busy(void)
{
    bool busy;
    int slot;

    mutex_lock(&photo_mutex);
    busy = photo_catalog_refreshing || photo_worker_decoding ||
        (!photo_suspended && view_slot.pending);
    for(slot = 0;
        !busy && !photo_suspended &&
        slot < CRAZYPOD_PHOTO_THUMB_SLOTS;
        ++slot)
        busy = thumbnail_slots[slot].pending;
    mutex_unlock(&photo_mutex);
    return busy;
}

bool crazypod_photos_catalog_refreshing(void)
{
    bool refreshing;

    mutex_lock(&photo_mutex);
    refreshing = photo_catalog_refreshing;
    mutex_unlock(&photo_mutex);
    return refreshing;
}

#endif
