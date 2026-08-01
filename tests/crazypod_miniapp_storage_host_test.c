#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "crazypod_miniapps.h"
#include "crazypod_miniapp_storage.h"

int main(void)
{
    static const uint8_t state[] = { 1, 2, 3, 4, 5 };
    static const uint8_t file[] = { 9, 8, 7, 6 };
    uint8_t buffer[32];
    char exported[512];

    assert(crazypod_miniapp_storage_write(
               "test-app", state, sizeof(state)) ==
           CRAZYPOD_MINIAPP_OK);
    memset(buffer, 0, sizeof(buffer));
    assert(crazypod_miniapp_storage_read(
               "test-app", buffer, sizeof(buffer)) ==
           (int)sizeof(state));
    assert(memcmp(buffer, state, sizeof(state)) == 0);

    assert(crazypod_miniapp_file_write(
               "test-app", "saves/slot-1.bin",
               file, sizeof(file)) == CRAZYPOD_MINIAPP_OK);
    assert(crazypod_miniapp_file_size(
               "test-app", "saves/slot-1.bin") ==
           (int)sizeof(file));
    memset(buffer, 0, sizeof(buffer));
    assert(crazypod_miniapp_file_read(
               "test-app", "saves/slot-1.bin",
               buffer, sizeof(buffer)) == (int)sizeof(file));
    assert(memcmp(buffer, file, sizeof(file)) == 0);
    assert(crazypod_miniapp_file_remove(
               "test-app", "saves/slot-1.bin") ==
           CRAZYPOD_MINIAPP_OK);

    assert(crazypod_miniapp_file_write(
               "test-app", "../state.bin",
               file, sizeof(file)) == CRAZYPOD_MINIAPP_ERROR_STATE);
    assert(crazypod_miniapp_file_write(
               "test-app", "/.rockbox/config.cfg",
               file, sizeof(file)) == CRAZYPOD_MINIAPP_ERROR_STATE);
    assert(crazypod_miniapp_file_write(
               "TEST", "file.bin",
               file, sizeof(file)) == CRAZYPOD_MINIAPP_ERROR_STATE);

    assert(crazypod_miniapp_user_file_export(
               "test-app", "result.bin",
               file, sizeof(file),
               exported, sizeof(exported)) ==
           CRAZYPOD_MINIAPP_OK);
    assert(crazypod_miniapp_user_file_size(exported) ==
           (int)sizeof(file));
    memset(buffer, 0, sizeof(buffer));
    assert(crazypod_miniapp_user_file_read(
               exported, 1, buffer, 2) == 2);
    assert(buffer[0] == 8 && buffer[1] == 7);
    assert(crazypod_miniapp_user_file_export(
               "test-app", "../result.bin",
               file, sizeof(file),
               exported, sizeof(exported)) ==
           CRAZYPOD_MINIAPP_ERROR_STATE);
    assert(crazypod_miniapp_user_file_size(
               "/.rockbox/config.cfg") ==
           CRAZYPOD_MINIAPP_ERROR_STATE);
    assert(crazypod_miniapp_user_file_read(
               "/.crazypod/private.bin", 0,
               buffer, sizeof(buffer)) ==
           CRAZYPOD_MINIAPP_ERROR_STATE);
    return 0;
}
