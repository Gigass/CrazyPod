#ifndef CRAZYPOD_ARTWORK_H
#define CRAZYPOD_ARTWORK_H

#include <stdbool.h>

#include "lvgl.h"
#include "crazypod_music.h"

#define CRAZYPOD_ARTWORK_SLOTS 20

void crazypod_artwork_init(void);
void crazypod_artwork_prime_library(void);
void crazypod_artwork_cancel_library_prime(void);
bool crazypod_artwork_library_priming(void);
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
unsigned crazypod_artwork_generation(void);
unsigned crazypod_artwork_slot_generation(int slot);
bool crazypod_artwork_busy(void);

#endif
