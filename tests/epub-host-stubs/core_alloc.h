#ifndef EPUB_HOST_TEST_CORE_ALLOC_H
#define EPUB_HOST_TEST_CORE_ALLOC_H

#include <stdlib.h>

struct buflib_callbacks {
    int unused;
};

static const struct buflib_callbacks buflib_ops_locked = {0};
static void *host_core_allocations[128];

static inline int core_alloc_ex(
    size_t size, const struct buflib_callbacks *callbacks)
{
    int handle;
    (void)callbacks;

    for(handle = 0; handle < 128; ++handle) {
        if(host_core_allocations[handle] == NULL) {
            host_core_allocations[handle] = malloc(size > 0 ? size : 1);
            return host_core_allocations[handle] != NULL ? handle : -1;
        }
    }
    return -1;
}

static inline void *core_get_data(int handle)
{
    return handle >= 0 && handle < 128
        ? host_core_allocations[handle] : NULL;
}

static inline void core_free(int handle)
{
    if(handle >= 0 && handle < 128) {
        free(host_core_allocations[handle]);
        host_core_allocations[handle] = NULL;
    }
}

#endif
