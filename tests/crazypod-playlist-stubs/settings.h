#ifndef TEST_CRAZYPOD_PLAYLIST_SETTINGS_H
#define TEST_CRAZYPOD_PLAYLIST_SETTINGS_H

#include <stdbool.h>

enum {
    REPEAT_OFF = 0,
    REPEAT_ALL,
    REPEAT_ONE,
};

struct user_settings {
    int repeat_mode;
    bool playlist_shuffle;
};

struct system_status {
    int resume_index;
    unsigned long resume_elapsed;
    unsigned long resume_offset;
};

extern struct user_settings global_settings;
extern struct system_status global_status;

#endif
