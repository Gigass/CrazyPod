#include "config.h"

#ifdef IPOD_6G

#include "dir.h"
#include "general.h"
#include "screendump.h"

#include "crazypod_photos.h"
#include "crazypod_screenshot.h"

#define SCREENSHOT_ALBUM "/Pictures/Screenshots"

bool crazypod_screenshot_capture(void)
{
    char path[MAX_PATH];

    mkdir("/Pictures");
    mkdir(SCREENSHOT_ALBUM);
    if(create_numbered_filename(
           path, SCREENSHOT_ALBUM,
           "Screenshot_", ".bmp", 4
           IF_CNFN_NUM_(, NULL)) == NULL)
        return false;
    if(!screen_dump_to_file(path))
        return false;
    crazypod_photos_note_file_added();
    return true;
}

#endif
