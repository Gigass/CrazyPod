#include <assert.h>

#include "crazypod_music.h"

int main(void)
{
    /* A clean boot can trust an atomically committed, checksummed cache. */
    assert(crazypod_music_catalog_validation_after_boot(true) ==
           CRAZYPOD_MUSIC_VALIDATION_CURRENT);
    assert(crazypod_music_catalog_validation_after_boot(false) ==
           CRAZYPOD_MUSIC_VALIDATION_FAILED);

    /* A USB remount must still validate because the host may change media. */
    assert(crazypod_music_catalog_validation_after_mount(true) ==
           CRAZYPOD_MUSIC_VALIDATION_UNCHECKED);
    assert(crazypod_music_catalog_validation_after_mount(false) ==
           CRAZYPOD_MUSIC_VALIDATION_FAILED);
    assert(crazypod_music_library_needs_validation(
               true, CRAZYPOD_MUSIC_VALIDATION_UNCHECKED));
    assert(!crazypod_music_library_needs_validation(
                true, CRAZYPOD_MUSIC_VALIDATION_CURRENT));
    assert(!crazypod_music_library_needs_validation(
                false, CRAZYPOD_MUSIC_VALIDATION_UNCHECKED));

    /* Disk Mode is intentionally stricter than a clean boot. */
    assert(crazypod_music_catalog_validation_after_mount(true) !=
           CRAZYPOD_MUSIC_VALIDATION_CURRENT);
    return 0;
}
