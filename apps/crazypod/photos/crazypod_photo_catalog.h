#ifndef CRAZYPOD_PHOTO_CATALOG_H
#define CRAZYPOD_PHOTO_CATALOG_H

#include <stdbool.h>
#include <stdint.h>

#include "config.h"

struct crazypod_photo_catalog_entry {
    char path[MAX_PATH];
    uint32_t size;
    uint32_t mtime;
    uint32_t key;
    bool favorite;
};

bool crazypod_photo_catalog_init(void);
void crazypod_photo_catalog_refresh(void);
void crazypod_photo_catalog_invalidate(void);
int crazypod_photo_catalog_count(void);
int crazypod_photo_catalog_favorite_count(void);
int crazypod_photo_catalog_favorite_index(int favorite_index);
const struct crazypod_photo_catalog_entry *
crazypod_photo_catalog_get(int index);
const char *crazypod_photo_catalog_name(int index);
bool crazypod_photo_catalog_toggle_favorite(int index);
uint32_t crazypod_photo_catalog_key(const char *path);
bool crazypod_photo_catalog_path_supported(const char *path);

#endif
