#include "config.h"

#ifdef IPOD_6G

#include <stdbool.h>
#include <string.h>

#include "abrepeat.h"
#include "cuesheet.h"
#include "misc.h"
#include "settings.h"
#include "sound.h"
#include "talk.h"
#include "voice_thread.h"
#include "string-extra.h"

#include "crazypod_music.h"

struct user_settings global_settings;
struct system_status global_status;

int string_option(const char *option, const char *const options[],
                  bool ignore_case)
{
    int i;

    if(option == NULL || options == NULL)
        return -1;

    for(i = 0; options[i] != NULL; ++i) {
        int result = ignore_case
            ? strcasecmp(option, options[i])
            : strcmp(option, options[i]);
        if(result == 0)
            return i;
    }

    return -1;
}

void system_sound_play(enum system_sound sound)
{
    (void)sound;
}

char *strip_extension(char *buffer, int buffer_size, const char *filename)
{
    const char *slash;
    const char *dot;
    size_t length;

    if(buffer == NULL || filename == NULL || buffer_size <= 0)
        return NULL;

    slash = strrchr(filename, '/');
    dot = strrchr(filename, '.');
    if(dot == NULL || (slash != NULL && dot < slash) ||
       dot == (slash != NULL ? slash + 1 : filename))
        length = strlen(filename);
    else
        length = (size_t)(dot - filename);

    if(length >= (size_t)buffer_size)
        length = (size_t)buffer_size - 1;
    memcpy(buffer, filename, length);
    buffer[length] = '\0';
    return buffer;
}

void fix_path_part(char *path, int offset, int count)
{
    static const char invalid[] = "*/:<>?\\|";
    int i;

    if(path == NULL || offset < 0 || count <= 0)
        return;
    for(i = offset; path[i] != '\0' && i < offset + count; ++i) {
        if(path[i] == '"')
            path[i] = '\'';
        else if(strchr(invalid, path[i]) != NULL)
            path[i] = '_';
    }
}

#ifdef AB_REPEAT_ENABLE
unsigned int ab_A_marker;
unsigned int ab_B_marker;

bool ab_before_A_marker(unsigned int position)
{
    return ab_A_marker != AB_MARKER_NONE && position < ab_A_marker;
}

bool ab_after_A_marker(unsigned int position)
{
    return ab_A_marker != AB_MARKER_NONE && position > ab_A_marker;
}

void ab_jump_to_A_marker(void)
{
}

void ab_reset_markers(void)
{
    ab_A_marker = AB_MARKER_NONE;
    ab_B_marker = AB_MARKER_NONE;
}

void ab_set_A_marker(unsigned int position)
{
    ab_A_marker = position;
}

void ab_set_B_marker(unsigned int position)
{
    ab_B_marker = position;
}

bool ab_get_A_marker(unsigned int *position)
{
    if(position != NULL)
        *position = ab_A_marker;
    return ab_A_marker != AB_MARKER_NONE;
}

bool ab_get_B_marker(unsigned int *position)
{
    if(position != NULL)
        *position = ab_B_marker;
    return ab_B_marker != AB_MARKER_NONE;
}

void ab_end_of_track_report(void)
{
}
#endif

bool look_for_cuesheet_file(struct mp3entry *id3,
                            struct cuesheet_file *cue_file)
{
    (void)id3;
    (void)cue_file;
    return false;
}

bool parse_cuesheet(struct cuesheet_file *cue_file, struct cuesheet *cue)
{
    (void)cue_file;
    (void)cue;
    return false;
}

void voice_stop(void)
{
}

#ifdef HAVE_PRIORITY_SCHEDULING
void voice_thread_set_priority(int priority)
{
    (void)priority;
}
#endif

void talk_buffer_set_policy(int policy)
{
    (void)policy;
}

void talk_force_shutup(void)
{
}

void sound_settings_apply(void)
{
#ifdef AUDIOHW_HAVE_BASS
    sound_set(SOUND_BASS, global_settings.bass);
#endif
#ifdef AUDIOHW_HAVE_TREBLE
    sound_set(SOUND_TREBLE, global_settings.treble);
#endif
    sound_set(SOUND_BALANCE, global_settings.balance);
#ifndef PLATFORM_HAS_VOLUME_CHANGE
    sound_set(SOUND_VOLUME, global_status.volume);
#endif
    sound_set(SOUND_CHANNELS, global_settings.channel_config);
    sound_set(SOUND_STEREO_WIDTH, global_settings.stereo_width);
}

void crazypod_audio_settings_init(void)
{
    memset(&global_settings, 0, sizeof(global_settings));
    memset(&global_status, 0, sizeof(global_status));
    global_status.volume = -25;
    global_status.resume_index = -1;
    global_settings.stereo_width = 100;
    global_settings.repeat_mode = REPEAT_OFF;
    global_settings.single_mode = SINGLE_MODE_OFF;
    global_settings.max_files_in_playlist = CRAZYPOD_MAX_TRACKS;
#ifdef HAVE_DISK_STORAGE
    global_settings.buffer_margin = 5;
#endif
#ifdef HAVE_ALBUMART
    global_settings.album_art = AA_PREFER_EMBEDDED;
#endif
#ifdef HAVE_CROSSFADE
    global_settings.crossfade = CROSSFADE_ENABLE_OFF;
#endif
#ifdef AB_REPEAT_ENABLE
    ab_reset_markers();
#endif
}

#endif
