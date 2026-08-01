#ifndef CRAZYPOD_MINIAPP_STORAGE_H
#define CRAZYPOD_MINIAPP_STORAGE_H

#include <stddef.h>

#define CRAZYPOD_MINIAPP_STORAGE_MAX (1024u * 1024u)
#define CRAZYPOD_MINIAPP_FILE_MAX (2u * 1024u * 1024u)
#define CRAZYPOD_MINIAPP_USER_CHUNK_MAX (1024u * 1024u)
#define CRAZYPOD_MINIAPP_EXPORT_MAX (8u * 1024u * 1024u)

int crazypod_miniapp_storage_read(
    const char *id, void *buffer, size_t capacity);
int crazypod_miniapp_storage_write(
    const char *id, const void *buffer, size_t size);
int crazypod_miniapp_file_size(
    const char *id, const char *relative_path);
int crazypod_miniapp_file_read(
    const char *id, const char *relative_path,
    void *buffer, size_t capacity);
int crazypod_miniapp_file_write(
    const char *id, const char *relative_path,
    const void *buffer, size_t size);
int crazypod_miniapp_file_remove(
    const char *id, const char *relative_path);
int crazypod_miniapp_user_file_size(const char *path);
int crazypod_miniapp_user_file_read(
    const char *path, uint32_t offset,
    void *buffer, size_t capacity);
int crazypod_miniapp_user_file_export(
    const char *id, const char *filename,
    const void *buffer, size_t size,
    char *output_path, size_t output_capacity);

#endif
