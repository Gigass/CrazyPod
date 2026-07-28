#include <string.h>

#include "crazypod_miniapp_catalog.h"

static struct crazypod_miniapp_metadata
    entries[CRAZYPOD_MINIAPP_MAX_APPS];
static int entry_count;

void crazypod_miniapp_catalog_reset(void)
{
    entry_count = 0;
}

bool crazypod_miniapp_catalog_add(
    const struct crazypod_miniapp_metadata *metadata)
{
    if(metadata == NULL || entry_count >= CRAZYPOD_MINIAPP_MAX_APPS)
        return false;
    entries[entry_count++] = *metadata;
    return true;
}

void crazypod_miniapp_catalog_sort(void)
{
    int index;

    for(index = 1; index < entry_count; ++index) {
        struct crazypod_miniapp_metadata value = entries[index];
        int position = index;

        while(position > 0 &&
              strcmp(entries[position - 1].id, value.id) > 0) {
            entries[position] = entries[position - 1];
            --position;
        }
        entries[position] = value;
    }
}

int crazypod_miniapp_catalog_count(void)
{
    return entry_count;
}

const struct crazypod_miniapp_metadata *
crazypod_miniapp_catalog_get(int index)
{
    if(index < 0 || index >= entry_count)
        return NULL;
    return &entries[index];
}

int crazypod_miniapp_catalog_find(const char *id)
{
    int index;

    if(id == NULL)
        return -1;
    for(index = 0; index < entry_count; ++index)
        if(strcmp(entries[index].id, id) == 0)
            return index;
    return -1;
}
