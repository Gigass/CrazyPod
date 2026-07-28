#ifndef CRAZYPOD_MINIAPP_STORAGE_H
#define CRAZYPOD_MINIAPP_STORAGE_H

#include <stddef.h>

int crazypod_miniapp_storage_read(
    const char *id, void *buffer, size_t capacity);
int crazypod_miniapp_storage_write(
    const char *id, const void *buffer, size_t size);

#endif
