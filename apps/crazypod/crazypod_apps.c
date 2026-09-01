#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "crazypod_apps.h"

static const uint8_t default_order[CRAZYPOD_APP_COUNT] = {
    CRAZYPOD_APP_MUSIC,
    CRAZYPOD_APP_PODCASTS,
    CRAZYPOD_APP_MINI_APPS,
    CRAZYPOD_APP_SHUFFLE,
    CRAZYPOD_APP_LOCK,
    CRAZYPOD_APP_PHOTOS,
    CRAZYPOD_APP_CUSTOMIZE,
    CRAZYPOD_APP_WORKOUTS,
    CRAZYPOD_APP_BOOKS,
    CRAZYPOD_APP_NOTES,
    CRAZYPOD_APP_CLOCK,
    CRAZYPOD_APP_CONTACTS,
    CRAZYPOD_APP_CALENDAR,
    CRAZYPOD_APP_STOPWATCH,
    CRAZYPOD_APP_EXTRAS,
    CRAZYPOD_APP_SETTINGS,
    CRAZYPOD_APP_GAMEBOY,
};

static uint8_t menu_order[CRAZYPOD_APP_COUNT];
static uint32_t menu_enabled_mask;

static uint32_t app_bit(enum crazypod_app_id id)
{
    return id > CRAZYPOD_APP_INVALID && id < 32
        ? (uint32_t)1u << (unsigned)id : 0;
}

bool crazypod_apps_is_known(enum crazypod_app_id id)
{
    int i;

    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i) {
        if(default_order[i] == (uint8_t)id)
            return true;
    }
    return false;
}

bool crazypod_apps_is_fixed(enum crazypod_app_id id)
{
    return id == CRAZYPOD_APP_MUSIC ||
           id == CRAZYPOD_APP_CUSTOMIZE ||
           id == CRAZYPOD_APP_EXTRAS ||
           id == CRAZYPOD_APP_SETTINGS ||
           id == CRAZYPOD_APP_GAMEBOY;
}

void crazypod_apps_reset(void)
{
    int i;

    memcpy(menu_order, default_order, sizeof(menu_order));
    menu_enabled_mask = 0;
    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i)
        if(menu_order[i] != CRAZYPOD_APP_LOCK &&
           menu_order[i] != CRAZYPOD_APP_CLOCK &&
           menu_order[i] != CRAZYPOD_APP_CONTACTS &&
           menu_order[i] != CRAZYPOD_APP_CALENDAR &&
           menu_order[i] != CRAZYPOD_APP_STOPWATCH)
            menu_enabled_mask |=
                app_bit((enum crazypod_app_id)menu_order[i]);
}

void crazypod_apps_restore(const uint8_t *order, size_t count,
                           uint32_t enabled_mask)
{
    bool seen[32];
    int output = 0;
    size_t i;

    memset(seen, 0, sizeof(seen));
    if(order != NULL) {
        for(i = 0; i < count && output < CRAZYPOD_APP_COUNT; ++i) {
            enum crazypod_app_id id = (enum crazypod_app_id)order[i];
            if(id <= CRAZYPOD_APP_INVALID || id >= 32 ||
               seen[id] || !crazypod_apps_is_known(id))
                continue;
            menu_order[output++] = (uint8_t)id;
            seen[id] = true;
        }
    }
    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i) {
        enum crazypod_app_id id =
            (enum crazypod_app_id)default_order[i];
        if(!seen[id]) {
            menu_order[output++] = (uint8_t)id;
            seen[id] = true;
        }
    }

    menu_enabled_mask = enabled_mask;
    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i) {
        enum crazypod_app_id id =
            (enum crazypod_app_id)default_order[i];
        if(crazypod_apps_is_fixed(id))
            menu_enabled_mask |= app_bit(id);
    }
}

void crazypod_apps_export(uint8_t *order, size_t capacity,
                          uint32_t *enabled_mask)
{
    size_t count = capacity < CRAZYPOD_APP_COUNT
        ? capacity : CRAZYPOD_APP_COUNT;

    if(order != NULL && count > 0)
        memcpy(order, menu_order, count);
    if(enabled_mask != NULL)
        *enabled_mask = menu_enabled_mask;
}

int crazypod_apps_count(void)
{
    return CRAZYPOD_APP_COUNT;
}

enum crazypod_app_id crazypod_apps_ordered_id(int index)
{
    if(index < 0 || index >= CRAZYPOD_APP_COUNT)
        return CRAZYPOD_APP_INVALID;
    return (enum crazypod_app_id)menu_order[index];
}

int crazypod_apps_order_index(enum crazypod_app_id id)
{
    int i;

    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i) {
        if(menu_order[i] == (uint8_t)id)
            return i;
    }
    return -1;
}

bool crazypod_apps_is_enabled(enum crazypod_app_id id)
{
    return crazypod_apps_is_known(id) &&
           (menu_enabled_mask & app_bit(id)) != 0;
}

int crazypod_apps_visible_count(void)
{
    int count = 0;
    int i;

    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i) {
        if(crazypod_apps_is_enabled(
               (enum crazypod_app_id)menu_order[i]))
            ++count;
    }
    return count;
}

enum crazypod_app_id crazypod_apps_visible_id(int index)
{
    int visible = 0;
    int i;

    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i) {
        enum crazypod_app_id id =
            (enum crazypod_app_id)menu_order[i];
        if(!crazypod_apps_is_enabled(id))
            continue;
        if(visible++ == index)
            return id;
    }
    return CRAZYPOD_APP_INVALID;
}

int crazypod_apps_visible_index(enum crazypod_app_id id)
{
    int visible = 0;
    int i;

    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i) {
        enum crazypod_app_id current =
            (enum crazypod_app_id)menu_order[i];
        if(!crazypod_apps_is_enabled(current))
            continue;
        if(current == id)
            return visible;
        ++visible;
    }
    return -1;
}

int crazypod_apps_hidden_count(void)
{
    return CRAZYPOD_APP_COUNT - crazypod_apps_visible_count();
}

enum crazypod_app_id crazypod_apps_hidden_id(int index)
{
    int hidden = 0;
    int i;

    for(i = 0; i < CRAZYPOD_APP_COUNT; ++i) {
        enum crazypod_app_id id =
            (enum crazypod_app_id)menu_order[i];
        if(crazypod_apps_is_enabled(id))
            continue;
        if(hidden++ == index)
            return id;
    }
    return CRAZYPOD_APP_INVALID;
}

bool crazypod_apps_set_enabled(enum crazypod_app_id id, bool enabled)
{
    uint32_t bit;
    uint32_t before;

    if(!crazypod_apps_is_known(id) ||
       (!enabled && crazypod_apps_is_fixed(id)))
        return false;
    bit = app_bit(id);
    before = menu_enabled_mask;
    if(enabled)
        menu_enabled_mask |= bit;
    else
        menu_enabled_mask &= ~bit;
    return before != menu_enabled_mask;
}

bool crazypod_apps_move(enum crazypod_app_id id, int direction)
{
    int index = crazypod_apps_order_index(id);
    int target = index + (direction < 0 ? -1 : 1);
    uint8_t swap;

    if(index < 0 || direction == 0 ||
       target < 0 || target >= CRAZYPOD_APP_COUNT)
        return false;
    swap = menu_order[target];
    menu_order[target] = menu_order[index];
    menu_order[index] = swap;
    return true;
}

#endif
