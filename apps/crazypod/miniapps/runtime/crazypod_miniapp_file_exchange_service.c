#include "config.h"

#ifdef IPOD_6G

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../crazypod_miniapps.h"
#include "../crazypod_miniapp_storage.h"
#include "crazypod_miniapp_file_exchange_service.h"

static bool valid_text(const char *text, size_t capacity)
{
    return text != NULL && memchr(text, '\0', capacity) != NULL;
}

static int native_storage_error(int status)
{
    if(status == CRAZYPOD_MINIAPP_ERROR_LIMIT ||
       status == CRAZYPOD_MINIAPP_ERROR_SPACE)
        return CP_NATIVE_ERROR_LIMIT;
    if(status == CRAZYPOD_MINIAPP_ERROR_UNSUPPORTED)
        return CP_NATIVE_ERROR_UNSUPPORTED;
    if(status == CRAZYPOD_MINIAPP_ERROR_STATE ||
       status == CRAZYPOD_MINIAPP_ERROR_BUSY)
        return CP_NATIVE_ERROR_STATE;
    if(status == CRAZYPOD_MINIAPP_ERROR_FORMAT ||
       status == CRAZYPOD_MINIAPP_ERROR_MANIFEST)
        return CP_NATIVE_ERROR_ARGUMENT;
    return CP_NATIVE_ERROR_IO;
}

static int stat_file(
    const struct cp_file_exchange_read_request *request,
    size_t request_size, void *response, size_t response_capacity)
{
    int32_t size;

    if(request == NULL || request_size != sizeof(*request) ||
       request->struct_size != sizeof(*request) ||
       !valid_text(request->path, sizeof(request->path)) ||
       response == NULL || response_capacity < sizeof(size))
        return CP_NATIVE_ERROR_ARGUMENT;
    size = crazypod_miniapp_user_file_size(request->path);
    if(size < 0)
        return native_storage_error(size);
    memcpy(response, &size, sizeof(size));
    return (int)sizeof(size);
}

static int read_text(
    const struct cp_file_exchange_read_request *request,
    size_t request_size, void *response, size_t response_capacity)
{
    struct cp_file_exchange_text_result result;
    int total;
    int amount;

    if(request == NULL || request_size != sizeof(*request) ||
       request->struct_size != sizeof(*request) ||
       !valid_text(request->path, sizeof(request->path)) ||
       response == NULL || response_capacity < sizeof(result))
        return CP_NATIVE_ERROR_ARGUMENT;
    total = crazypod_miniapp_user_file_size(request->path);
    if(total < 0)
        return native_storage_error(total);
    if(request->offset > (uint32_t)total)
        return CP_NATIVE_ERROR_ARGUMENT;
    memset(&result, 0, sizeof(result));
    result.struct_size = sizeof(result);
    amount = crazypod_miniapp_user_file_read(
        request->path, request->offset, result.text,
        sizeof(result.text) - 1u);
    if(amount < 0)
        return native_storage_error(amount);
    if(memchr(result.text, '\0', (size_t)amount) != NULL)
        return CP_NATIVE_ERROR_ARGUMENT;
    result.size = (uint32_t)amount;
    result.eof = request->offset + (uint32_t)amount >= (uint32_t)total;
    result.text[amount] = '\0';
    memcpy(response, &result, sizeof(result));
    return (int)sizeof(result);
}

static int export_text(
    const struct crazypod_miniapp_metadata *metadata,
    const struct cp_file_exchange_export_request *request,
    size_t request_size, void *response, size_t response_capacity)
{
    struct cp_file_exchange_export_result result;
    size_t size;
    int status;

    if(metadata == NULL || request == NULL ||
       request_size != sizeof(*request) ||
       request->struct_size != sizeof(*request) ||
       !valid_text(request->filename, sizeof(request->filename)) ||
       !valid_text(request->text, sizeof(request->text)) ||
       response == NULL || response_capacity < sizeof(result))
        return CP_NATIVE_ERROR_ARGUMENT;
    memset(&result, 0, sizeof(result));
    result.struct_size = sizeof(result);
    size = strlen(request->text);
    status = crazypod_miniapp_user_file_export(
        metadata->id, request->filename, request->text, size,
        result.path, sizeof(result.path));
    if(status != CRAZYPOD_MINIAPP_OK)
        return native_storage_error(status);
    memcpy(response, &result, sizeof(result));
    return (int)sizeof(result);
}

int crazypod_miniapp_file_exchange_service_call(
    const struct crazypod_miniapp_metadata *metadata,
    uint32_t operation, const void *request, size_t request_size,
    void *response, size_t response_capacity)
{
    if(metadata == NULL)
        return CP_NATIVE_ERROR_STATE;
    if(operation == CP_NATIVE_FILE_EXCHANGE_STAT ||
       operation == CP_NATIVE_FILE_EXCHANGE_READ_TEXT) {
        if((metadata->permissions &
            CRAZYPOD_MINIAPP_PERMISSION_USER_FILES_READ) == 0)
            return CP_NATIVE_ERROR_UNSUPPORTED;
        return operation == CP_NATIVE_FILE_EXCHANGE_STAT
            ? stat_file(request, request_size, response, response_capacity)
            : read_text(request, request_size, response, response_capacity);
    }
    if(operation == CP_NATIVE_FILE_EXCHANGE_EXPORT_TEXT) {
        if((metadata->permissions &
            CRAZYPOD_MINIAPP_PERMISSION_USER_FILES_EXPORT) == 0)
            return CP_NATIVE_ERROR_UNSUPPORTED;
        return export_text(metadata, request, request_size,
                           response, response_capacity);
    }
    return CP_NATIVE_ERROR_UNSUPPORTED;
}

#endif
