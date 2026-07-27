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

static const struct eq_band_setting crazypod_eq_defaults[EQ_NUM_BANDS] = {
    { 32, 7, 0 },
    { 64, 10, 0 },
    { 125, 10, 0 },
    { 250, 10, 0 },
    { 500, 10, 0 },
    { 1000, 10, 0 },
    { 2000, 10, 0 },
    { 4000, 10, 0 },
    { 8000, 10, 0 },
    { 16000, 7, 0 },
};

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
    static const struct beep_params
    {
        int *setting;
        unsigned short frequency;
        unsigned short duration;
        unsigned short amplitude;
    } beep_params[] =
    {
        [SOUND_KEYCLICK] =
        { &global_settings.keyclick, 4000, KEYCLICK_DURATION, 2500 },
        [SOUND_TRACK_SKIP] =
        { &global_settings.beep, 2000, 100, 2500 },
        [SOUND_TRACK_NO_MORE] =
        { &global_settings.beep, 1000, 100, 1500 },
        [SOUND_LIST_EDGE_BEEP_WRAP] =
        { &global_settings.keyclick, 2000, 20, 1500 },
        [SOUND_LIST_EDGE_BEEP_NOWRAP] =
        { &global_settings.keyclick, 1000, 40, 1500 },
    };
    const struct beep_params *params;

    if(sound > SOUND_LIST_EDGE_BEEP_NOWRAP)
        return;

    params = &beep_params[sound];
    if(*params->setting)
        beep_play(params->frequency, params->duration,
                  params->amplitude * *params->setting);
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

void crazypod_eq_settings_apply(void)
{
    int i;

    dsp_eq_enable(global_settings.eq_enabled);
    dsp_set_eq_precut(global_settings.eq_precut);
    for(i = 0; i < EQ_NUM_BANDS; ++i)
        dsp_set_eq_coefs(i, &global_settings.eq_band_settings[i]);
}

void crazypod_audio_settings_init(void)
{
    int i;

    memset(&global_settings, 0, sizeof(global_settings));
    memset(&global_status, 0, sizeof(global_status));
    global_status.volume = -25;
    global_status.resume_index = -1;
    global_settings.stereo_width = 100;
    global_settings.repeat_mode = REPEAT_OFF;
    global_settings.single_mode = SINGLE_MODE_OFF;
    global_settings.max_files_in_playlist = CRAZYPOD_MAX_TRACKS;
#ifdef HAVE_BACKLIGHT
    global_settings.backlight_timeout = 30;
#if CONFIG_CHARGING
    global_settings.backlight_timeout_plugged = 60;
#endif
#endif
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    global_settings.brightness = DEFAULT_BRIGHTNESS_SETTING;
#endif
#ifdef HAVE_LCD_SLEEP_SETTING
    global_settings.lcd_sleep_after_backlight_off = 1;
#endif
    global_settings.sleeptimer_duration = 30;
    for(i = 0; i < EQ_NUM_BANDS; ++i)
        global_settings.eq_band_settings[i] = crazypod_eq_defaults[i];
#ifdef HAVE_DISK_STORAGE
    global_settings.buffer_margin = 5;
#endif
#ifdef HAVE_ALBUMART
    global_settings.album_art = AA_PREFER_EMBEDDED;
#endif
#ifdef HAVE_CROSSFADE
    global_settings.crossfade = CROSSFADE_ENABLE_OFF;
#endif
#ifdef HAVE_HARDWARE_CLICK
    global_settings.keyclick_hardware = true;
#endif
#ifdef AB_REPEAT_ENABLE
    ab_reset_markers();
#endif
}

#endif
