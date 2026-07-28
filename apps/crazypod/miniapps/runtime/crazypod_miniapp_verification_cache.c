#include <string.h>

#include "../../crazypod_miniapps.h"
#include "crazypod_miniapp_verification_cache.h"

static struct {
    char id[CRAZYPOD_MINIAPP_ID_SIZE];
    uint32_t version_code;
} entries[CRAZYPOD_MINIAPP_MAX_APPS];
static int entry_count;

void crazypod_miniapp_verification_cache_clear(void)
{
    memset(entries, 0, sizeof(entries));
    entry_count = 0;
}

bool crazypod_miniapp_verification_cache_contains(
    const char *id, uint32_t version_code)
{
    int index;
    for(index = 0; index < entry_count; ++index)
        if(entries[index].version_code == version_code &&
           strcmp(entries[index].id, id) == 0)
            return true;
    return false;
}

void crazypod_miniapp_verification_cache_mark(
    const char *id, uint32_t version_code)
{
    size_t length;
    int index;

    if(id == NULL || version_code == 0)
        return;
    length = strlen(id);
    if(length == 0 || length >= CRAZYPOD_MINIAPP_ID_SIZE)
        return;
    for(index = 0; index < entry_count; ++index) {
        if(strcmp(entries[index].id, id) == 0) {
            entries[index].version_code = version_code;
            return;
        }
    }
    if(entry_count >= CRAZYPOD_MINIAPP_MAX_APPS)
        return;
    memcpy(entries[entry_count].id, id, length + 1);
    entries[entry_count].version_code = version_code;
    ++entry_count;
}
