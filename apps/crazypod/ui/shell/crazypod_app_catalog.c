#include "config.h"

#ifdef IPOD_6G

#include "lvgl.h"

#include "crazypod_app_catalog.h"

static const struct crazypod_app_descriptor catalog[CRAZYPOD_APP_COUNT] = {
    { CRAZYPOD_APP_MUSIC, "Music", LV_SYMBOL_AUDIO, 0xFF2E54 },
    { CRAZYPOD_APP_PODCASTS, "Podcasts", LV_SYMBOL_VOLUME_MAX, 0xA95BDE },
    { CRAZYPOD_APP_MINI_APPS, "Mini Apps", LV_SYMBOL_LIST, 0xFF9F0A },
    { CRAZYPOD_APP_SHUFFLE, "Shuffle", LV_SYMBOL_SHUFFLE, 0xFF375F },
    { CRAZYPOD_APP_LOCK, "Lock", LV_SYMBOL_EYE_CLOSE, 0x59606B },
    { CRAZYPOD_APP_PHOTOS, "Media", LV_SYMBOL_IMAGE, 0x3478F6 },
    { CRAZYPOD_APP_CUSTOMIZE, "Customize", LV_SYMBOL_EDIT, 0xBF5AF2 },
    { CRAZYPOD_APP_WORKOUTS, "Workouts", LV_SYMBOL_PLAY, 0xA8F12D },
    { CRAZYPOD_APP_BOOKS, "Books", LV_SYMBOL_FILE, 0xFF9F0A },
    { CRAZYPOD_APP_NOTES, "Notes", LV_SYMBOL_EDIT, 0xFFD60A },
    { CRAZYPOD_APP_CLOCK, "Clock", LV_SYMBOL_HOME, 0xF26D5B },
    { CRAZYPOD_APP_CONTACTS, "Contacts", LV_SYMBOL_HOME, 0x4F9BFF },
    { CRAZYPOD_APP_CALENDAR, "Calendar", LV_SYMBOL_LIST, 0xFF453A },
    { CRAZYPOD_APP_STOPWATCH, "Stopwatch", LV_SYMBOL_REFRESH, 0xFFB340 },
    { CRAZYPOD_APP_EXTRAS, "More Features", LV_SYMBOL_DIRECTORY, 0x64D2FF },
    { CRAZYPOD_APP_SETTINGS, "Settings", LV_SYMBOL_SETTINGS, 0x8E8E93 },
};

const struct crazypod_app_descriptor *crazypod_app_catalog_at(int index)
{
    return index >= 0 && index < CRAZYPOD_APP_COUNT
        ? &catalog[index] : NULL;
}

int crazypod_app_catalog_index(enum crazypod_app_id id)
{
    int i;

    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i) {
        if(catalog[i].id == id)
            return i;
    }
    return -1;
}

const struct crazypod_app_descriptor *crazypod_app_catalog_find(
    enum crazypod_app_id id)
{
    return crazypod_app_catalog_at(crazypod_app_catalog_index(id));
}

#endif
