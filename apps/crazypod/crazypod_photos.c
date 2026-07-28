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
static unsigned photo_view_publish_generation;
static bool photo_worker_decoding;
static bool photo_suspended;
static bool photo_wake_queued;
static struct mutex photo_mutex;
static struct event_queue photo_queue;
static long photo_stack[CRAZYPOD_PHOTO_THREAD_STACK_SIZE /
                        sizeof(long)];

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
        if(!thumbnail_slots[slot].pending)
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
        photo_worker_decoding = true;
        mutex_unlock(&photo_mutex);
        return true;
    }
    photo_worker_decoding = false;
    photo_wake_queued = false;
    mutex_unlock(&photo_mutex);
    return false;
}

static void publish_request(const struct photo_decode_request *request,
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
            ++photo_publish_generation;
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
            ++photo_publish_generation;
        }
    }
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

static void finish_request(void)
{
    mutex_lock(&photo_mutex);
    photo_worker_decoding = false;
    mutex_unlock(&photo_mutex);
}

static void photo_thread(void)
{
    struct queue_event event;

    while(true) {
        queue_wait(&photo_queue, &event);
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
            publish_request(&request, bank, valid);
            if(valid && request.view && !cache_hit)
                crazypod_photo_cache_store(
                    true, request.path, request.size, request.mtime,
                    descriptor);
            else if(valid && !request.view && !cache_hit)
                crazypod_photo_cache_store(
                    false, request.path, request.size, request.mtime,
                    descriptor);
            finish_request();
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

void crazypod_photos_init(void)
{
    int slot;

    memset(thumbnail_slots, 0, sizeof(thumbnail_slots));
    memset(&view_slot, 0, sizeof(view_slot));
    for(slot = 0; slot < CRAZYPOD_PHOTO_THUMB_SLOTS; ++slot) {
        thumbnail_slots[slot].requested_index = -1;
        thumbnail_slots[slot].decoded_index = -1;
    }
    view_slot.requested_index = -1;
    view_slot.decoded_index = -1;
    mutex_init(&photo_mutex);
    queue_init(&photo_queue, false);
    create_thread(photo_thread, photo_stack, sizeof(photo_stack), 0,
                  "crazypod photos"
                  IF_PRIO(, PRIORITY_USER_INTERFACE)
                  IF_COP(, CPU));
    crazypod_photo_catalog_init();
    crazypod_photo_cache_init();
    crazypod_photos_refresh();
}

void crazypod_photos_refresh(void)
{
    mutex_lock(&photo_mutex);
    photo_suspended = true;
    mutex_unlock(&photo_mutex);
    while(photo_worker_decoding)
        yield();
    crazypod_photo_catalog_refresh();
    mutex_lock(&photo_mutex);
    memset(thumbnail_slots, 0, sizeof(thumbnail_slots));
    memset(&view_slot, 0, sizeof(view_slot));
    view_slot.requested_index = -1;
    view_slot.decoded_index = -1;
    photo_suspended = false;
    photo_wake_queued = false;
    ++photo_publish_generation;
    ++photo_view_publish_generation;
    crazypod_photo_viewport_reset();
    mutex_unlock(&photo_mutex);
}

void crazypod_photos_suspend(void)
{
    mutex_lock(&photo_mutex);
    photo_suspended = true;
    mutex_unlock(&photo_mutex);
    while(photo_worker_decoding)
        yield();
}

void crazypod_photos_resume(void)
{
    mutex_lock(&photo_mutex);
    photo_suspended = false;
    photo_wake_queued = false;
    mutex_unlock(&photo_mutex);
    crazypod_photos_refresh();
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
    const struct crazypod_photo_catalog_entry *entry =
        crazypod_photo_catalog_get(index);

    return entry != NULL ? entry->path : "";
}

const char *crazypod_photo_name(int index)
{
    return crazypod_photo_catalog_name(index);
}

bool crazypod_photo_is_favorite(int index)
{
    const struct crazypod_photo_catalog_entry *entry =
        crazypod_photo_catalog_get(index);

    return entry != NULL && entry->favorite;
}

bool crazypod_photo_toggle_favorite(int index)
{
    if(!crazypod_photo_catalog_toggle_favorite(index))
        return false;
    ++photo_publish_generation;
    return true;
}

const lv_image_dsc_t *crazypod_photo_thumbnail(int slot_index, int index)
{
    const struct crazypod_photo_catalog_entry *entry =
        crazypod_photo_catalog_get(index);
    struct photo_slot *slot;
    const lv_image_dsc_t *result = NULL;
    bool changed = false;

    if(slot_index < 0 || slot_index >= CRAZYPOD_PHOTO_THUMB_SLOTS ||
       entry == NULL)
        return NULL;
    mutex_lock(&photo_mutex);
    slot = &thumbnail_slots[slot_index];
    if(slot->decoded_index == index &&
       slot->decoded_serial == slot->request_serial &&
       slot->valid &&
       strcmp(slot->requested_path, entry->path) == 0) {
        result = &slot->descriptor[slot->active_bank];
    }
    else if(slot->requested_index != index ||
            slot->requested_size != entry->size ||
            slot->requested_mtime != entry->mtime ||
            strcmp(slot->requested_path, entry->path) != 0) {
        slot->requested_index = index;
        slot->requested_size = entry->size;
        slot->requested_mtime = entry->mtime;
        snprintf(slot->requested_path, sizeof(slot->requested_path),
                 "%s", entry->path);
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
    const struct crazypod_photo_catalog_entry *entry =
        crazypod_photo_catalog_get(index);
    const lv_image_dsc_t *result = NULL;
    bool changed = false;

    if(entry == NULL)
        return NULL;
    mutex_lock(&photo_mutex);
    if(view_slot.decoded_index == index &&
       view_slot.decoded_serial == view_slot.request_serial &&
       view_slot.valid &&
       strcmp(view_slot.requested_path, entry->path) == 0) {
        result = &view_slot.descriptor[view_slot.active_bank];
    }
    else if(view_slot.requested_index != index ||
            view_slot.requested_size != entry->size ||
            view_slot.requested_mtime != entry->mtime ||
            strcmp(view_slot.requested_path, entry->path) != 0) {
        view_slot.requested_index = index;
        view_slot.requested_size = entry->size;
        view_slot.requested_mtime = entry->mtime;
        snprintf(view_slot.requested_path,
                 sizeof(view_slot.requested_path), "%s",
                 entry->path);
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
    const struct crazypod_photo_catalog_entry *entry =
        crazypod_photo_catalog_get(index);
    int progress;

    if(entry == NULL)
        return -1;
    mutex_lock(&photo_mutex);
    if(view_slot.requested_index != index ||
       strcmp(view_slot.requested_path, entry->path) != 0)
        progress = 0;
    else
        progress = view_slot.progress;
    mutex_unlock(&photo_mutex);
    return progress;
}

static const lv_image_dsc_t *ready_thumbnail(int index)
{
    const struct crazypod_photo_catalog_entry *entry =
        crazypod_photo_catalog_get(index);
    const lv_image_dsc_t *result = NULL;
    int slot_index;

    if(entry == NULL)
        return NULL;
    mutex_lock(&photo_mutex);
    for(slot_index = 0;
        slot_index < CRAZYPOD_PHOTO_THUMB_SLOTS; ++slot_index) {
        struct photo_slot *slot = &thumbnail_slots[slot_index];

        if(slot->decoded_index == index && slot->valid &&
           strcmp(slot->requested_path, entry->path) == 0) {
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
    int index, int center_y)
{
    return crazypod_photo_viewport_render_crop(
        crazypod_photo_view(index), center_y);
}

unsigned crazypod_photo_generation(void)
{
    unsigned generation;

    mutex_lock(&photo_mutex);
    generation = photo_publish_generation;
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
    busy = photo_worker_decoding || view_slot.pending;
    for(slot = 0; !busy && slot < CRAZYPOD_PHOTO_THUMB_SLOTS; ++slot)
        busy = thumbnail_slots[slot].pending;
    mutex_unlock(&photo_mutex);
    return busy;
}

#endif
