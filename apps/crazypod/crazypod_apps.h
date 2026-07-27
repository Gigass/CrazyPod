#ifndef CRAZYPOD_APPS_H
#define CRAZYPOD_APPS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * These values are persisted. Retired IDs stay reserved so an older state
 * file can never turn Camera or Voice Memos into a different application.
 */
enum crazypod_app_id {
    CRAZYPOD_APP_INVALID = 0,
    CRAZYPOD_APP_MUSIC = 1,
    CRAZYPOD_APP_PODCASTS = 2,
    CRAZYPOD_APP_MINI_APPS = 3,
    CRAZYPOD_APP_SHUFFLE = 4,
    CRAZYPOD_APP_LOCK = 5,
    CRAZYPOD_APP_CAMERA_RETIRED = 6,
    CRAZYPOD_APP_PHOTOS = 7,
    CRAZYPOD_APP_CUSTOMIZE = 8,
    CRAZYPOD_APP_WORKOUTS = 9,
    CRAZYPOD_APP_VOICE_MEMOS_RETIRED = 10,
    CRAZYPOD_APP_BOOKS = 11,
    CRAZYPOD_APP_NOTES = 12,
    CRAZYPOD_APP_EXTRAS = 13,
    CRAZYPOD_APP_SETTINGS = 14,
    CRAZYPOD_APP_CLOCK = 15,
    CRAZYPOD_APP_CONTACTS = 16,
    CRAZYPOD_APP_CALENDAR = 17,
    CRAZYPOD_APP_STOPWATCH = 18,
};

#define CRAZYPOD_APP_COUNT 16

void crazypod_apps_reset(void);
void crazypod_apps_restore(const uint8_t *order, size_t count,
                           uint32_t enabled_mask);
void crazypod_apps_export(uint8_t *order, size_t capacity,
                          uint32_t *enabled_mask);

int crazypod_apps_count(void);
enum crazypod_app_id crazypod_apps_ordered_id(int index);
int crazypod_apps_order_index(enum crazypod_app_id id);

int crazypod_apps_visible_count(void);
enum crazypod_app_id crazypod_apps_visible_id(int index);
int crazypod_apps_visible_index(enum crazypod_app_id id);

int crazypod_apps_hidden_count(void);
enum crazypod_app_id crazypod_apps_hidden_id(int index);

bool crazypod_apps_is_known(enum crazypod_app_id id);
bool crazypod_apps_is_fixed(enum crazypod_app_id id);
bool crazypod_apps_is_enabled(enum crazypod_app_id id);
bool crazypod_apps_set_enabled(enum crazypod_app_id id, bool enabled);
bool crazypod_apps_move(enum crazypod_app_id id, int direction);

#endif
