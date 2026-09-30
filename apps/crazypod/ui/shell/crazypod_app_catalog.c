#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef IPOD_6G

#include "lvgl.h"

#include "crazypod_app_catalog.h"

static const struct crazypod_app_descriptor catalog[CRAZYPOD_APP_COUNT] = {
    { CRAZYPOD_APP_MUSIC, CP_TR("Music"), LV_SYMBOL_AUDIO,
      CRAZYPOD_MENU_ICON_MUSIC, 0xFF2E54 },
    { CRAZYPOD_APP_PODCASTS, CP_TR("Podcasts"), LV_SYMBOL_VOLUME_MAX,
      CRAZYPOD_MENU_ICON_PODCAST, 0xA95BDE },
    { CRAZYPOD_APP_MINI_APPS, CP_TR("Mini Apps"), LV_SYMBOL_LIST,
      CRAZYPOD_MENU_ICON_APPS, 0xFF9F0A },
    { CRAZYPOD_APP_SHUFFLE, CP_TR("Shuffle"), LV_SYMBOL_SHUFFLE,
      CRAZYPOD_MENU_ICON_SHUFFLE, 0xFF375F },
    { CRAZYPOD_APP_LOCK, CP_TR("Lock"), LV_SYMBOL_EYE_CLOSE,
      CRAZYPOD_MENU_ICON_LOCK, 0x59606B },
    { CRAZYPOD_APP_PHOTOS, CP_TR("Media"), LV_SYMBOL_IMAGE,
      CRAZYPOD_MENU_ICON_PHOTO, 0x3478F6 },
    { CRAZYPOD_APP_CUSTOMIZE, CP_TR("Customize"), LV_SYMBOL_EDIT,
      CRAZYPOD_MENU_ICON_CUSTOMIZE, 0xBF5AF2 },
    { CRAZYPOD_APP_WORKOUTS, CP_TR("Workouts"), LV_SYMBOL_PLAY,
      CRAZYPOD_MENU_ICON_WORKOUT, 0xA8F12D },
    { CRAZYPOD_APP_BOOKS, CP_TR("Books"), LV_SYMBOL_FILE,
      CRAZYPOD_MENU_ICON_BOOK, 0xFF9F0A },
    { CRAZYPOD_APP_NOTES, CP_TR("Notes"), LV_SYMBOL_EDIT,
      CRAZYPOD_MENU_ICON_NOTE, 0xFFD60A },
    { CRAZYPOD_APP_CLOCK, CP_TR("Clock"), LV_SYMBOL_HOME,
      CRAZYPOD_MENU_ICON_CLOCK, 0xF26D5B },
    { CRAZYPOD_APP_CONTACTS, CP_TR("Contacts"), LV_SYMBOL_HOME,
      CRAZYPOD_MENU_ICON_CONTACT, 0x4F9BFF },
    { CRAZYPOD_APP_CALENDAR, CP_TR("Calendar"), LV_SYMBOL_LIST,
      CRAZYPOD_MENU_ICON_CALENDAR, 0xFF453A },
    { CRAZYPOD_APP_STOPWATCH, CP_TR("Stopwatch"), LV_SYMBOL_REFRESH,
      CRAZYPOD_MENU_ICON_STOPWATCH, 0xFFB340 },
    { CRAZYPOD_APP_EXTRAS, CP_TR("More Features"), LV_SYMBOL_DIRECTORY,
      CRAZYPOD_MENU_ICON_MORE, 0x64D2FF },
    { CRAZYPOD_APP_SETTINGS, CP_TR("Settings"), LV_SYMBOL_SETTINGS,
      CRAZYPOD_MENU_ICON_SETTINGS, 0x8E8E93 },
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
