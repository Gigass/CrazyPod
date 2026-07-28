#ifndef CRAZYPOD_MINIAPP_CATALOG_H
#define CRAZYPOD_MINIAPP_CATALOG_H

#include <stdbool.h>

#include "../../crazypod_miniapps.h"

void crazypod_miniapp_catalog_reset(void);
bool crazypod_miniapp_catalog_add(
    const struct crazypod_miniapp_metadata *metadata);
void crazypod_miniapp_catalog_sort(void);
int crazypod_miniapp_catalog_count(void);
const struct crazypod_miniapp_metadata *
crazypod_miniapp_catalog_get(int index);
int crazypod_miniapp_catalog_find(const char *id);

#endif
