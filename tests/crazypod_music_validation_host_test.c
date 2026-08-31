#include <assert.h>

#include "crazypod_music.h"

int main(void)
{
    /* A cache survives a reboot, not a media validation. */
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

    /* Disk Mode uses the same post-mount state as a clean boot. */
    assert(crazypod_music_catalog_validation_after_mount(true) !=
           CRAZYPOD_MUSIC_VALIDATION_CURRENT);
    return 0;
}
