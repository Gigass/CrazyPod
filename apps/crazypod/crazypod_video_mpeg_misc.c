#include "config.h"

#ifdef IPOD_6G

#define CRAZYPOD_VIDEO_CORE 1

#include <stdint.h>
#include <stdio.h>

#include "crazypod_video_plugin.h"
#include "../plugins/mpegplayer/mpegplayer.h"

void stream_scan_init(struct stream_scan *scan)
{
    dbuf_l2_init(&scan->l2);
}

void stream_scan_normalize(struct stream_scan *scan)
{
    if(scan->dir >= 0) {
        scan->dir = SSCAN_FORWARD;
        scan->margin = scan->len;
    }
    else {
        scan->dir = SSCAN_REVERSE;
        scan->margin = 0;
    }
}

void stream_scan_offset(struct stream_scan *scan, off_t amount)
{
    off_t directed = amount * scan->dir;

    scan->pos += directed;
    scan->margin -= directed;
    scan->len -= amount;
}

void ts_to_hms(uint32_t timestamp, struct hms *time)
{
    time->frac = timestamp % TS_SECOND;
    time->sec = timestamp / TS_SECOND;
    time->min = time->sec / 60;
    time->hrs = time->min / 60;
    time->sec %= 60;
    time->min %= 60;
}

void hms_format(char *buffer, size_t size, struct hms *time)
{
    if(time->hrs != 0)
        snprintf(buffer, size, "%u:%02u:%02u",
                 time->hrs, time->min, time->sec);
    else
        snprintf(buffer, size, "%u:%02u", time->min, time->sec);
}

uint32_t muldiv_uint32(uint32_t multiplicand, uint32_t multiplier,
                       uint32_t divisor)
{
    if(divisor != 0) {
        uint64_t product =
            (uint64_t)multiplier * multiplicand + divisor / 2;

        if((uint32_t)(product >> 32) < divisor)
            return (uint32_t)(product / divisor);
    }
    else if(multiplicand == 0 || multiplier == 0)
        return 0;

    return UINT32_MAX;
}

bool list_is_empty(void **list)
{
    return *list == NULL;
}

bool list_is_member(void **list, void *item)
{
    while(*list != NULL) {
        if(*list == item)
            return true;
        ++list;
    }
    return false;
}

bool list_remove_item(void **list, void *item)
{
    void **cursor = list;

    while(*cursor != NULL && *cursor != item)
        ++cursor;
    if(*cursor == NULL)
        return false;
    do {
        *cursor = *(cursor + 1);
        ++cursor;
    } while(*cursor != NULL);
    return true;
}

void list_add_item(void **list, void *item)
{
    while(*list != NULL) {
        if(*list == item)
            return;
        ++list;
    }
    *list = item;
}

void list_clear_all(void **list)
{
    while(*list != NULL)
        *list++ = NULL;
}

void list_enum_items(void **list, list_enum_callback_t callback, void *data)
{
    for(;;) {
        void *item = *list;

        if(item == NULL)
            break;
        if(callback != NULL && !callback(item, data))
            break;
        if(*list == item)
            ++list;
    }
}

void mpeg_sysevent_clear(void)
{
}

void mpeg_sysevent_set(void)
{
}

long mpeg_sysevent(void)
{
    return 0;
}

int mpeg_sysevent_callback(int button,
                           const struct menu_item_ex *menu,
                           struct gui_synclist *list)
{
    (void)menu;
    (void)list;
    return button;
}

void mpeg_sysevent_handle(void)
{
}

int mpeg_button_get(int timeout)
{
    return timeout == TIMEOUT_BLOCK
        ? button_get(true) : button_get_w_tmo(timeout);
}

#endif
