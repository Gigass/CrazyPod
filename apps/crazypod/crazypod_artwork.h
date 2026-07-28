#ifndef CRAZYPOD_ARTWORK_H
#define CRAZYPOD_ARTWORK_H

#include <stdbool.h>

#include "lvgl.h"
#include "crazypod_music.h"

#define CRAZYPOD_COVERFLOW_ARTWORK_SLOTS 25
#define CRAZYPOD_PREVIEW_ARTWORK_SLOT \
    CRAZYPOD_COVERFLOW_ARTWORK_SLOTS
#define CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT \
    (CRAZYPOD_PREVIEW_ARTWORK_SLOT + 1)
#define CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT \
    (CRAZYPOD_NOW_PREFETCH_ARTWORK_SLOT + 1)
#define CRAZYPOD_CAPSULE_ARTWORK_SLOT \
    (CRAZYPOD_NOW_PLAYING_ARTWORK_SLOT + 1)
#define CRAZYPOD_ARTWORK_SLOTS \
    (CRAZYPOD_CAPSULE_ARTWORK_SLOT + 1)
#define CRAZYPOD_COVERFLOW_ARTWORK_SIZE 128
#define CRAZYPOD_PREVIEW_ARTWORK_SIZE 120
#define CRAZYPOD_CAPSULE_ARTWORK_SIZE 42

enum crazypod_artwork_state {
    CRAZYPOD_ARTWORK_PENDING,
    CRAZYPOD_ARTWORK_IMAGE,
    CRAZYPOD_ARTWORK_EMPTY,
};

void crazypod_artwork_init(void);
void crazypod_artwork_prime_library(void);
void crazypod_artwork_cancel_library_prime(void);
bool crazypod_artwork_library_priming(void);
bool crazypod_artwork_library_prime_failed(void);
int crazypod_artwork_library_prime_completed(void);
int crazypod_artwork_library_prime_total(void);
void crazypod_artwork_suspend(void);
void crazypod_artwork_resume(void);
const lv_image_dsc_t *crazypod_artwork_load(int slot,
                                            const struct crazypod_track *track,
                                            int target_size);
const lv_image_dsc_t *crazypod_artwork_load_priority(
    int slot, const struct crazypod_track *track, int target_size,
    int priority);
const lv_image_dsc_t *crazypod_artwork_load_cached_priority(
    int slot, const struct crazypod_track *track, int target_size,
    int priority);
const lv_image_dsc_t *crazypod_artwork_load_coverflow_priority(
    int slot, const struct crazypod_track *track, int priority);
enum crazypod_artwork_state crazypod_artwork_state(
    int slot, const struct crazypod_track *track, int target_size);
unsigned crazypod_artwork_generation(void);
unsigned crazypod_artwork_slot_generation(int slot);
bool crazypod_artwork_busy(void);

#endif
