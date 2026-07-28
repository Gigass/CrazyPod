#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>

#include "file.h"

#include "crazypod_miniapp_native_validator.h"

#if CONFIG_BINFMT == BINFMT_ROCK
extern unsigned char pluginbuf[];

static bool read_at_exact(
    int fd, uint32_t offset, void *buffer, size_t size)
{
    uint8_t *cursor = buffer;
    if(lseek(fd, (off_t)offset, SEEK_SET) < 0)
        return false;
    while(size > 0) {
        ssize_t count = read(fd, cursor, size);
        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}
#endif

bool crazypod_miniapp_native_header_valid(
    const struct miniapp_binary_header_runtime *header,
    uint32_t file_size)
{
#if CONFIG_BINFMT == BINFMT_ROCK
    uintptr_t load;
    uintptr_t end;
    uintptr_t bss;
    uintptr_t entry;

    if(header == NULL ||
       header->lc_header.magic != CP_MINIAPP_BINARY_MAGIC ||
       header->lc_header.target_id != TARGET_ID ||
       header->lc_header.api_version != CP_MINIAPP_ABI_VERSION ||
       header->host_api_size < CP_HOST_API_V1_SIZE ||
       header->host_api_size > sizeof(struct cp_host_api) ||
       header->ops_size != sizeof(struct cp_miniapp_ops) ||
       header->entry == NULL)
        return false;
    load = (uintptr_t)header->lc_header.load_addr;
    end = (uintptr_t)header->lc_header.end_addr;
    bss = (uintptr_t)header->bss_start;
    entry = (uintptr_t)header->entry;
    if(load != (uintptr_t)pluginbuf || end <= load ||
       end - load > PLUGIN_BUFFER_SIZE ||
       file_size > PLUGIN_BUFFER_SIZE ||
       bss < load || bss > end ||
       file_size > bss - load ||
       entry < load || entry >= bss)
        return false;
#else
    (void)header;
    (void)file_size;
#endif
    return true;
}

int crazypod_miniapp_native_package_validate(
    const struct cpk_reader *reader)
{
#if CONFIG_BINFMT == BINFMT_ROCK
    struct miniapp_binary_header_runtime header;
    const struct cpk_entry *entry = &reader->entries[CPK_BINARY];

    if(entry->size < sizeof(header) ||
       !read_at_exact(reader->fd, entry->data_offset,
                      &header, sizeof(header)))
        return CRAZYPOD_MINIAPP_ERROR_FORMAT;
    if(!crazypod_miniapp_native_header_valid(&header, entry->size))
        return CRAZYPOD_MINIAPP_ERROR_ABI;
#else
    (void)reader;
#endif
    return CRAZYPOD_MINIAPP_OK;
}

int crazypod_miniapp_native_installed_validate(
    const struct crazypod_miniapp_metadata *metadata,
    uint32_t *file_size_out)
{
#if CONFIG_BINFMT == BINFMT_ROCK
    struct miniapp_binary_header_runtime header;
    int fd = open(metadata->binary_path, O_RDONLY);
    off_t size;
    bool valid;

    if(fd < 0)
        return CRAZYPOD_MINIAPP_ERROR_IO;
    size = filesize(fd);
    valid = size >= (off_t)sizeof(header) &&
            size <= (off_t)PLUGIN_BUFFER_SIZE &&
            read_at_exact(fd, 0, &header, sizeof(header)) &&
            crazypod_miniapp_native_header_valid(
                &header, (uint32_t)size);
    close(fd);
    if(valid && file_size_out != NULL)
        *file_size_out = (uint32_t)size;
    return valid ? CRAZYPOD_MINIAPP_OK
                 : CRAZYPOD_MINIAPP_ERROR_ABI;
#else
    (void)metadata;
    if(file_size_out != NULL)
        *file_size_out = 0;
    return CRAZYPOD_MINIAPP_OK;
#endif
}

#endif
