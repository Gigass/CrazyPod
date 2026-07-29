#include "config.h"

#ifdef IPOD_6G

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "audio.h"
#include "backlight.h"
#include "dir.h"
#include "file.h"
#include "kernel.h"
#include "metadata.h"
#include "powermgmt.h"
#include "settings.h"
#include "sound.h"
#include "storage.h"
#include "usb.h"

#include "crazypod_audio_shims.h"
#include "crazypod_apps.h"
#include "crazypod_checksum.h"
#include "crazypod_playlist.h"
#include "crazypod_state.h"

#define STATE_DIRECTORY "/.crazypod"
#define STATE_PATH STATE_DIRECTORY "/state.bin"
#define STATE_TEMP_PATH STATE_DIRECTORY "/state.tmp"
#define QUEUE_PATH STATE_DIRECTORY "/queue.m3u8"
#define QUEUE_TEMP_PATH STATE_DIRECTORY "/queue.tmp"
#define STATE_MAGIC 0x43505354u
#define STATE_VERSION 9u
#define STATE_SAVE_INTERVAL (30 * HZ)
#define STATE_SAVE_RETRY_INTERVAL (30 * HZ)
#define STATE_SAVE_MAX_RETRY_SHIFT 3
#define CRAZYPOD_EQ_GAIN_MIN (-240)
#define CRAZYPOD_EQ_GAIN_MAX 240
#define CRAZYPOD_EQ_Q_MIN 1
#define CRAZYPOD_EQ_Q_MAX 64
#define CRAZYPOD_EQ_CUTOFF_MIN 20
#define CRAZYPOD_EQ_CUTOFF_MAX 22040

struct crazypod_state_disk_v1 {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    int32_t volume;
    int32_t repeat_mode;
    uint32_t shuffled;
    int32_t queue_index;
    uint32_t queue_count;
    uint32_t queue_hash;
    uint32_t elapsed;
    uint32_t checksum;
};

struct crazypod_state_disk_v2 {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    int32_t volume;
    int32_t repeat_mode;
    uint32_t shuffled;
    int32_t queue_index;
    uint32_t queue_count;
    uint32_t queue_hash;
    uint32_t elapsed;
    int32_t eq_enabled;
    int32_t bass;
    int32_t treble;
    int32_t balance;
    int32_t brightness;
    int32_t backlight_timeout;
    int32_t backlight_timeout_plugged;
    int32_t lcd_sleep_after_backlight_off;
    int32_t sleeptimer_duration;
    int32_t sleeptimer_on_startup;
    int32_t keypress_restarts_sleeptimer;
    int32_t beep;
    int32_t keyclick;
    int32_t keyclick_repeats;
    uint32_t checksum;
};

struct crazypod_state_disk_v3 {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    int32_t volume;
    int32_t repeat_mode;
    uint32_t shuffled;
    int32_t queue_index;
    uint32_t queue_count;
    uint32_t queue_hash;
    uint32_t elapsed;
    int32_t eq_enabled;
    int32_t bass;
    int32_t treble;
    int32_t balance;
    int32_t brightness;
    int32_t backlight_timeout;
    int32_t backlight_timeout_plugged;
    int32_t lcd_sleep_after_backlight_off;
    int32_t sleeptimer_duration;
    int32_t sleeptimer_on_startup;
    int32_t keypress_restarts_sleeptimer;
    int32_t beep;
    int32_t keyclick;
    int32_t keyclick_repeats;
    int32_t keyclick_hardware;
    uint32_t checksum;
};

struct crazypod_state_eq_band_disk {
    int32_t cutoff;
    int32_t q;
    int32_t gain;
};

struct crazypod_state_disk_v4 {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    int32_t volume;
    int32_t repeat_mode;
    uint32_t shuffled;
    int32_t queue_index;
    uint32_t queue_count;
    uint32_t queue_hash;
    uint32_t elapsed;
    int32_t eq_enabled;
    int32_t bass;
    int32_t treble;
    int32_t balance;
    int32_t brightness;
    int32_t backlight_timeout;
    int32_t backlight_timeout_plugged;
    int32_t lcd_sleep_after_backlight_off;
    int32_t sleeptimer_duration;
    int32_t sleeptimer_on_startup;
    int32_t keypress_restarts_sleeptimer;
    int32_t beep;
    int32_t keyclick;
    int32_t keyclick_repeats;
    int32_t keyclick_hardware;
    int32_t eq_precut;
    struct crazypod_state_eq_band_disk eq_bands[EQ_NUM_BANDS];
    uint32_t checksum;
};

struct crazypod_state_disk_v5 {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    int32_t volume;
    int32_t repeat_mode;
    uint32_t shuffled;
    int32_t queue_index;
    uint32_t queue_count;
    uint32_t queue_hash;
    uint32_t elapsed;
    int32_t eq_enabled;
    int32_t bass;
    int32_t treble;
    int32_t balance;
    int32_t brightness;
    int32_t backlight_timeout;
    int32_t backlight_timeout_plugged;
    int32_t lcd_sleep_after_backlight_off;
    int32_t sleeptimer_duration;
    int32_t sleeptimer_on_startup;
    int32_t keypress_restarts_sleeptimer;
    int32_t usb_charging;
    int32_t beep;
    int32_t keyclick;
    int32_t keyclick_repeats;
    int32_t keyclick_hardware;
    int32_t eq_precut;
    struct crazypod_state_eq_band_disk eq_bands[EQ_NUM_BANDS];
    uint32_t checksum;
};

struct crazypod_state_disk_v6 {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    int32_t volume;
    int32_t repeat_mode;
    uint32_t shuffled;
    int32_t queue_index;
    uint32_t queue_count;
    uint32_t queue_hash;
    uint32_t elapsed;
    int32_t eq_enabled;
    int32_t bass;
    int32_t treble;
    int32_t balance;
    int32_t brightness;
    int32_t backlight_timeout;
    int32_t backlight_timeout_plugged;
    int32_t lcd_sleep_after_backlight_off;
    int32_t sleeptimer_duration;
    int32_t sleeptimer_on_startup;
    int32_t keypress_restarts_sleeptimer;
    int32_t usb_charging;
    int32_t beep;
    int32_t keyclick;
    int32_t keyclick_repeats;
    int32_t keyclick_hardware;
    int32_t eq_precut;
    struct crazypod_state_eq_band_disk eq_bands[EQ_NUM_BANDS];
    uint32_t menu_count;
    uint32_t menu_enabled_mask;
    uint8_t menu_order[12];
    uint32_t checksum;
};

struct crazypod_state_disk_v7 {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    int32_t volume;
    int32_t repeat_mode;
    uint32_t shuffled;
    int32_t queue_index;
    uint32_t queue_count;
    uint32_t queue_hash;
    uint32_t elapsed;
    int32_t eq_enabled;
    int32_t bass;
    int32_t treble;
    int32_t balance;
    int32_t brightness;
    int32_t backlight_timeout;
    int32_t backlight_timeout_plugged;
    int32_t lcd_sleep_after_backlight_off;
    int32_t sleeptimer_duration;
    int32_t sleeptimer_on_startup;
    int32_t keypress_restarts_sleeptimer;
    int32_t usb_charging;
    int32_t beep;
    int32_t keyclick;
    int32_t keyclick_repeats;
    int32_t keyclick_hardware;
    int32_t eq_precut;
    struct crazypod_state_eq_band_disk eq_bands[EQ_NUM_BANDS];
    uint32_t menu_count;
    uint32_t menu_enabled_mask;
    uint8_t menu_order[CRAZYPOD_APP_COUNT];
    uint32_t checksum;
};

struct crazypod_state_disk_v8 {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    int32_t volume;
    int32_t repeat_mode;
    uint32_t shuffled;
    int32_t queue_index;
    uint32_t queue_count;
    uint32_t queue_hash;
    uint32_t elapsed;
    int32_t eq_enabled;
    int32_t bass;
    int32_t treble;
    int32_t balance;
    int32_t brightness;
    int32_t backlight_timeout;
    int32_t backlight_timeout_plugged;
    int32_t lcd_sleep_after_backlight_off;
    int32_t sleeptimer_duration;
    int32_t sleeptimer_on_startup;
    int32_t keypress_restarts_sleeptimer;
    int32_t usb_charging;
    int32_t beep;
    int32_t keyclick;
    int32_t keyclick_repeats;
    int32_t keyclick_hardware;
    int32_t eq_precut;
    struct crazypod_state_eq_band_disk eq_bands[EQ_NUM_BANDS];
    uint32_t menu_count;
    uint32_t menu_enabled_mask;
    uint8_t menu_order[CRAZYPOD_APP_COUNT];
    int32_t reduce_motion;
    uint32_t checksum;
};

struct crazypod_state_disk {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    int32_t volume;
    int32_t repeat_mode;
    uint32_t shuffled;
    int32_t queue_index;
    uint32_t queue_count;
    uint32_t queue_hash;
    uint32_t elapsed;
    int32_t eq_enabled;
    int32_t bass;
    int32_t treble;
    int32_t balance;
    int32_t brightness;
    int32_t backlight_timeout;
    int32_t backlight_timeout_plugged;
    int32_t lcd_sleep_after_backlight_off;
    int32_t sleeptimer_duration;
    int32_t sleeptimer_on_startup;
    int32_t keypress_restarts_sleeptimer;
    int32_t usb_charging;
    int32_t beep;
    int32_t keyclick;
    int32_t keyclick_repeats;
    int32_t keyclick_hardware;
    int32_t eq_precut;
    struct crazypod_state_eq_band_disk eq_bands[EQ_NUM_BANDS];
    uint32_t menu_count;
    uint32_t menu_enabled_mask;
    uint8_t menu_order[CRAZYPOD_APP_COUNT];
    int32_t reduce_motion;
    int32_t storage_mode;
    uint32_t checksum;
};

static unsigned long resume_elapsed;
static unsigned long last_saved_elapsed;
static long last_save_tick;
static long last_save_attempt_tick;
static unsigned saved_queue_generation;
static uint32_t saved_queue_hash;
static uint32_t saved_queue_count;
static bool saved_queue_snapshot_valid;
static unsigned state_save_failures;
static bool state_dirty;
static bool reduce_motion;

static uint32_t hash_bytes(uint32_t hash, const void *data, size_t size)
{
    const unsigned char *bytes = data;
    size_t i;

    for(i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

static long state_save_retry_interval(void)
{
    unsigned shift =
        state_save_failures > 0 ? state_save_failures - 1 : 0;

    if(shift > STATE_SAVE_MAX_RETRY_SHIFT)
        shift = STATE_SAVE_MAX_RETRY_SHIFT;
    return STATE_SAVE_RETRY_INTERVAL << shift;
}

static void state_save_failed(void)
{
    if(state_save_failures <= STATE_SAVE_MAX_RETRY_SHIFT)
        ++state_save_failures;
}

static uint32_t state_checksum(const struct crazypod_state_disk *state)
{
    return crazypod_checksum_with_zeroed_u32(
        state, sizeof(*state),
        offsetof(struct crazypod_state_disk, checksum));
}

static uint32_t state_v1_checksum(const struct crazypod_state_disk_v1 *state)
{
    return crazypod_checksum_with_zeroed_u32(
        state, sizeof(*state),
        offsetof(struct crazypod_state_disk_v1, checksum));
}

static uint32_t state_v2_checksum(const struct crazypod_state_disk_v2 *state)
{
    return crazypod_checksum_with_zeroed_u32(
        state, sizeof(*state),
        offsetof(struct crazypod_state_disk_v2, checksum));
}

static uint32_t state_v3_checksum(const struct crazypod_state_disk_v3 *state)
{
    return crazypod_checksum_with_zeroed_u32(
        state, sizeof(*state),
        offsetof(struct crazypod_state_disk_v3, checksum));
}

static uint32_t state_v4_checksum(const struct crazypod_state_disk_v4 *state)
{
    return crazypod_checksum_with_zeroed_u32(
        state, sizeof(*state),
        offsetof(struct crazypod_state_disk_v4, checksum));
}

static uint32_t state_v5_checksum(const struct crazypod_state_disk_v5 *state)
{
    return crazypod_checksum_with_zeroed_u32(
        state, sizeof(*state),
        offsetof(struct crazypod_state_disk_v5, checksum));
}

static uint32_t state_v6_checksum(const struct crazypod_state_disk_v6 *state)
{
    return crazypod_checksum_with_zeroed_u32(
        state, sizeof(*state),
        offsetof(struct crazypod_state_disk_v6, checksum));
}

static uint32_t state_v7_checksum(const struct crazypod_state_disk_v7 *state)
{
    return crazypod_checksum_with_zeroed_u32(
        state, sizeof(*state),
        offsetof(struct crazypod_state_disk_v7, checksum));
}

static uint32_t state_v8_checksum(const struct crazypod_state_disk_v8 *state)
{
    return crazypod_checksum_with_zeroed_u32(
        state, sizeof(*state),
        offsetof(struct crazypod_state_disk_v8, checksum));
}

static bool read_exact(int fd, void *buffer, size_t size)
{
    unsigned char *bytes = buffer;
    size_t done = 0;

    while(done < size) {
        ssize_t count = read(fd, bytes + done, size - done);
        if(count <= 0)
            return false;
        done += (size_t)count;
    }
    return true;
}

static bool write_exact(int fd, const void *buffer, size_t size)
{
    const unsigned char *bytes = buffer;
    size_t done = 0;

    while(done < size) {
        ssize_t count = write(fd, bytes + done, size - done);
        if(count <= 0)
            return false;
        done += (size_t)count;
    }
    return true;
}

static int read_line(int fd, char *line, size_t size)
{
    size_t length = 0;
    char character;
    int result;

    while((result = read(fd, &character, 1)) == 1) {
        if(character == '\n')
            break;
        if(character == '\r')
            continue;
        if(length + 1 < size)
            line[length++] = character;
    }
    line[length] = '\0';
    return result == 1 || length > 0 ? (int)length : -1;
}

static void copy_current_eq_settings(struct crazypod_state_disk *state)
{
    int i;

    state->eq_precut = global_settings.eq_precut;
    for(i = 0; i < EQ_NUM_BANDS; ++i) {
        state->eq_bands[i].cutoff =
            global_settings.eq_band_settings[i].cutoff;
        state->eq_bands[i].q = global_settings.eq_band_settings[i].q;
        state->eq_bands[i].gain = global_settings.eq_band_settings[i].gain;
    }
}

static bool load_header(struct crazypod_state_disk *state)
{
    int fd = open(STATE_PATH, O_RDONLY);
    uint32_t header[3];
    bool valid = false;

    if(fd < 0)
        return false;
    if(!read_exact(fd, header, sizeof(header)) ||
       lseek(fd, 0, SEEK_SET) < 0) {
        close(fd);
        return false;
    }

    memset(state, 0, sizeof(*state));
    if(header[0] == STATE_MAGIC &&
       header[1] == STATE_VERSION &&
       header[2] == sizeof(*state)) {
        valid = read_exact(fd, state, sizeof(*state)) &&
                state->checksum == state_checksum(state);
    }
    else if(header[0] == STATE_MAGIC &&
            header[1] == 8u &&
            header[2] == sizeof(struct crazypod_state_disk_v8)) {
        struct crazypod_state_disk_v8 state_v8;

        valid = read_exact(fd, &state_v8, sizeof(state_v8)) &&
                state_v8.checksum == state_v8_checksum(&state_v8);
        if(valid) {
            memcpy(state, &state_v8,
                   offsetof(struct crazypod_state_disk_v8, checksum));
            state->magic = STATE_MAGIC;
            state->version = STATE_VERSION;
            state->size = sizeof(*state);
            state->storage_mode = 0;
        }
    }
    else if(header[0] == STATE_MAGIC &&
            header[1] == 7u &&
            header[2] == sizeof(struct crazypod_state_disk_v7)) {
        struct crazypod_state_disk_v7 state_v7;

        valid = read_exact(fd, &state_v7, sizeof(state_v7)) &&
                state_v7.checksum == state_v7_checksum(&state_v7);
        if(valid) {
            memcpy(state, &state_v7,
                   offsetof(struct crazypod_state_disk_v7, checksum));
            state->magic = STATE_MAGIC;
            state->version = STATE_VERSION;
            state->size = sizeof(*state);
            state->reduce_motion = 0;
        }
    }
    else if(header[0] == STATE_MAGIC &&
            header[1] == 6u &&
            header[2] == sizeof(struct crazypod_state_disk_v6)) {
        struct crazypod_state_disk_v6 state_v6;

        valid = read_exact(fd, &state_v6, sizeof(state_v6)) &&
                state_v6.checksum == state_v6_checksum(&state_v6);
        if(valid) {
            size_t menu_count = state_v6.menu_count < 12u
                ? state_v6.menu_count : 12u;
            memcpy(state, &state_v6,
                   offsetof(struct crazypod_state_disk_v6, menu_order));
            memcpy(state->menu_order, state_v6.menu_order, menu_count);
            state->magic = STATE_MAGIC;
            state->version = STATE_VERSION;
            state->size = sizeof(*state);
            state->menu_count = (uint32_t)menu_count;
        }
    }
    else if(header[0] == STATE_MAGIC &&
            header[1] == 5u &&
            header[2] == sizeof(struct crazypod_state_disk_v5)) {
        struct crazypod_state_disk_v5 state_v5;

        valid = read_exact(fd, &state_v5, sizeof(state_v5)) &&
                state_v5.checksum == state_v5_checksum(&state_v5);
        if(valid) {
            memcpy(state, &state_v5,
                   offsetof(struct crazypod_state_disk_v5, checksum));
            state->magic = STATE_MAGIC;
            state->version = STATE_VERSION;
            state->size = sizeof(*state);
        }
    }
    else if(header[0] == STATE_MAGIC &&
            header[1] == 4u &&
            header[2] == sizeof(struct crazypod_state_disk_v4)) {
        struct crazypod_state_disk_v4 state_v4;

        valid = read_exact(fd, &state_v4, sizeof(state_v4)) &&
                state_v4.checksum == state_v4_checksum(&state_v4);
        if(valid) {
            state->magic = STATE_MAGIC;
            state->version = STATE_VERSION;
            state->size = sizeof(*state);
            state->volume = state_v4.volume;
            state->repeat_mode = state_v4.repeat_mode;
            state->shuffled = state_v4.shuffled;
            state->queue_index = state_v4.queue_index;
            state->queue_count = state_v4.queue_count;
            state->queue_hash = state_v4.queue_hash;
            state->elapsed = state_v4.elapsed;
            state->eq_enabled = state_v4.eq_enabled;
            state->bass = state_v4.bass;
            state->treble = state_v4.treble;
            state->balance = state_v4.balance;
            state->brightness = state_v4.brightness;
            state->backlight_timeout = state_v4.backlight_timeout;
            state->backlight_timeout_plugged =
                state_v4.backlight_timeout_plugged;
            state->lcd_sleep_after_backlight_off =
                state_v4.lcd_sleep_after_backlight_off;
            state->sleeptimer_duration = state_v4.sleeptimer_duration;
            state->sleeptimer_on_startup = state_v4.sleeptimer_on_startup;
            state->keypress_restarts_sleeptimer =
                state_v4.keypress_restarts_sleeptimer;
            state->usb_charging = global_settings.usb_charging;
            state->beep = state_v4.beep;
            state->keyclick = state_v4.keyclick;
            state->keyclick_repeats = state_v4.keyclick_repeats;
            state->keyclick_hardware = state_v4.keyclick_hardware;
            state->eq_precut = state_v4.eq_precut;
            memcpy(state->eq_bands, state_v4.eq_bands,
                   sizeof(state->eq_bands));
        }
    }
    else if(header[0] == STATE_MAGIC &&
            header[1] == 3u &&
            header[2] == sizeof(struct crazypod_state_disk_v3)) {
        struct crazypod_state_disk_v3 state_v3;

        valid = read_exact(fd, &state_v3, sizeof(state_v3)) &&
                state_v3.checksum == state_v3_checksum(&state_v3);
        if(valid) {
            state->magic = STATE_MAGIC;
            state->version = STATE_VERSION;
            state->size = sizeof(*state);
            state->volume = state_v3.volume;
            state->repeat_mode = state_v3.repeat_mode;
            state->shuffled = state_v3.shuffled;
            state->queue_index = state_v3.queue_index;
            state->queue_count = state_v3.queue_count;
            state->queue_hash = state_v3.queue_hash;
            state->elapsed = state_v3.elapsed;
            state->eq_enabled = state_v3.eq_enabled;
            state->bass = state_v3.bass;
            state->treble = state_v3.treble;
            state->balance = state_v3.balance;
            state->brightness = state_v3.brightness;
            state->backlight_timeout = state_v3.backlight_timeout;
            state->backlight_timeout_plugged =
                state_v3.backlight_timeout_plugged;
            state->lcd_sleep_after_backlight_off =
                state_v3.lcd_sleep_after_backlight_off;
            state->sleeptimer_duration = state_v3.sleeptimer_duration;
            state->sleeptimer_on_startup = state_v3.sleeptimer_on_startup;
            state->keypress_restarts_sleeptimer =
                state_v3.keypress_restarts_sleeptimer;
            state->usb_charging = global_settings.usb_charging;
            state->beep = state_v3.beep;
            state->keyclick = state_v3.keyclick;
            state->keyclick_repeats = state_v3.keyclick_repeats;
            state->keyclick_hardware = state_v3.keyclick_hardware;
            copy_current_eq_settings(state);
        }
    }
    else if(header[0] == STATE_MAGIC &&
            header[1] == 2u &&
            header[2] == sizeof(struct crazypod_state_disk_v2)) {
        struct crazypod_state_disk_v2 state_v2;

        valid = read_exact(fd, &state_v2, sizeof(state_v2)) &&
                state_v2.checksum == state_v2_checksum(&state_v2);
        if(valid) {
            state->magic = STATE_MAGIC;
            state->version = STATE_VERSION;
            state->size = sizeof(*state);
            state->volume = state_v2.volume;
            state->repeat_mode = state_v2.repeat_mode;
            state->shuffled = state_v2.shuffled;
            state->queue_index = state_v2.queue_index;
            state->queue_count = state_v2.queue_count;
            state->queue_hash = state_v2.queue_hash;
            state->elapsed = state_v2.elapsed;
            state->eq_enabled = state_v2.eq_enabled;
            state->bass = state_v2.bass;
            state->treble = state_v2.treble;
            state->balance = state_v2.balance;
            state->brightness = state_v2.brightness;
            state->backlight_timeout = state_v2.backlight_timeout;
            state->backlight_timeout_plugged =
                state_v2.backlight_timeout_plugged;
            state->lcd_sleep_after_backlight_off =
                state_v2.lcd_sleep_after_backlight_off;
            state->sleeptimer_duration = state_v2.sleeptimer_duration;
            state->sleeptimer_on_startup = state_v2.sleeptimer_on_startup;
            state->keypress_restarts_sleeptimer =
                state_v2.keypress_restarts_sleeptimer;
            state->usb_charging = global_settings.usb_charging;
            state->beep = state_v2.beep;
            state->keyclick = state_v2.keyclick;
            state->keyclick_repeats = state_v2.keyclick_repeats;
            state->keyclick_hardware = 1;
            copy_current_eq_settings(state);
        }
    }
    else if(header[0] == STATE_MAGIC &&
            header[1] == 1u &&
            header[2] == sizeof(struct crazypod_state_disk_v1)) {
        struct crazypod_state_disk_v1 state_v1;

        valid = read_exact(fd, &state_v1, sizeof(state_v1)) &&
                state_v1.checksum == state_v1_checksum(&state_v1);
        if(valid) {
            state->magic = STATE_MAGIC;
            state->version = STATE_VERSION;
            state->size = sizeof(*state);
            state->volume = state_v1.volume;
            state->repeat_mode = state_v1.repeat_mode;
            state->shuffled = state_v1.shuffled;
            state->queue_index = state_v1.queue_index;
            state->queue_count = state_v1.queue_count;
            state->queue_hash = state_v1.queue_hash;
            state->elapsed = state_v1.elapsed;
            state->bass = global_settings.bass;
            state->treble = global_settings.treble;
            state->balance = global_settings.balance;
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
            state->brightness = global_settings.brightness;
#endif
            state->backlight_timeout = global_settings.backlight_timeout;
#if CONFIG_CHARGING
            state->backlight_timeout_plugged =
                global_settings.backlight_timeout_plugged;
#endif
            state->lcd_sleep_after_backlight_off =
                global_settings.lcd_sleep_after_backlight_off;
            state->sleeptimer_duration =
                global_settings.sleeptimer_duration;
            state->usb_charging = global_settings.usb_charging;
            state->beep = global_settings.beep;
            state->keyclick = global_settings.keyclick;
            state->keyclick_hardware = 1;
            copy_current_eq_settings(state);
        }
    }
    close(fd);

    return valid;
}

static int clamp_int(int value, int minimum, int maximum)
{
    if(value < minimum)
        return minimum;
    if(value > maximum)
        return maximum;
    return value;
}

static void clamp_and_apply_settings(const struct crazypod_state_disk *state)
{
    int volume = state->volume;
    int repeat = state->repeat_mode;
    int i;

    volume = clamp_int(volume, sound_min(SOUND_VOLUME),
                       sound_max(SOUND_VOLUME));
    if(repeat < REPEAT_OFF || repeat > REPEAT_ONE)
        repeat = REPEAT_OFF;

    global_status.volume = volume;
    global_settings.repeat_mode = repeat;
    global_settings.eq_enabled = state->eq_enabled != 0;
    global_settings.bass = clamp_int(state->bass, sound_min(SOUND_BASS),
                                     sound_max(SOUND_BASS));
    global_settings.treble = clamp_int(state->treble,
                                       sound_min(SOUND_TREBLE),
                                       sound_max(SOUND_TREBLE));
    global_settings.balance = clamp_int(state->balance,
                                        sound_min(SOUND_BALANCE),
                                        sound_max(SOUND_BALANCE));
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    global_settings.brightness =
        clamp_int(state->brightness, MIN_BRIGHTNESS_SETTING,
                  MAX_BRIGHTNESS_SETTING);
#endif
    global_settings.backlight_timeout =
        clamp_int(state->backlight_timeout, -1, 7200);
#if CONFIG_CHARGING
    global_settings.backlight_timeout_plugged =
        clamp_int(state->backlight_timeout_plugged, -1, 7200);
#endif
    global_settings.lcd_sleep_after_backlight_off =
        clamp_int(state->lcd_sleep_after_backlight_off, -1, 7200);
    if(global_settings.lcd_sleep_after_backlight_off == 0)
        global_settings.lcd_sleep_after_backlight_off = 1;
    global_settings.storage_mode =
        clamp_int(state->storage_mode, 0, 2);
    global_settings.sleeptimer_duration =
        clamp_int(state->sleeptimer_duration, 0, 300);
    global_settings.sleeptimer_on_startup =
        state->sleeptimer_on_startup != 0;
    global_settings.keypress_restarts_sleeptimer =
        state->keypress_restarts_sleeptimer != 0;
#ifdef HAVE_USB_CHARGING_ENABLE
    global_settings.usb_charging =
        clamp_int(state->usb_charging, USB_CHARGING_DISABLE,
                  USB_CHARGING_FORCE);
#endif
    global_settings.beep = clamp_int(state->beep, 0, 3);
    global_settings.keyclick = clamp_int(state->keyclick, 0, 3);
    global_settings.keyclick_repeats = state->keyclick_repeats != 0;
#ifdef HAVE_HARDWARE_CLICK
    global_settings.keyclick_hardware = state->keyclick_hardware != 0;
#endif
    global_settings.eq_precut = clamp_int(state->eq_precut, 0, 240);
    for(i = 0; i < EQ_NUM_BANDS; ++i) {
        global_settings.eq_band_settings[i].cutoff =
            clamp_int(state->eq_bands[i].cutoff,
                      CRAZYPOD_EQ_CUTOFF_MIN,
                      CRAZYPOD_EQ_CUTOFF_MAX);
        global_settings.eq_band_settings[i].q =
            clamp_int(state->eq_bands[i].q,
                      CRAZYPOD_EQ_Q_MIN,
                      CRAZYPOD_EQ_Q_MAX);
        global_settings.eq_band_settings[i].gain =
            clamp_int(state->eq_bands[i].gain,
                      CRAZYPOD_EQ_GAIN_MIN,
                      CRAZYPOD_EQ_GAIN_MAX);
    }

    sound_set_volume(volume);
    sound_settings_apply();
    crazypod_eq_settings_apply();
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    backlight_set_brightness(global_settings.brightness);
#endif
    backlight_set_timeout(global_settings.backlight_timeout);
#if CONFIG_CHARGING
    backlight_set_timeout_plugged(
        global_settings.backlight_timeout_plugged);
#endif
    lcd_set_sleep_after_backlight_off(
        global_settings.lcd_sleep_after_backlight_off);
    storage_set_storage_mode(global_settings.storage_mode);
    if(global_settings.sleeptimer_on_startup)
        set_sleeptimer_duration(global_settings.sleeptimer_duration);
    set_keypress_restarts_sleep_timer(
        global_settings.keypress_restarts_sleeptimer);
#ifdef HAVE_USB_CHARGING_ENABLE
    usb_charging_enable(global_settings.usb_charging);
#endif
}

void crazypod_state_load(void)
{
    struct crazypod_state_disk state;
    uint32_t queue_hash = 2166136261u;
    unsigned queue_count = 0;
    char line[MAX_PATH];
    int fd;

    resume_elapsed = 0;
    last_saved_elapsed = 0;
    last_save_tick = current_tick;
    last_save_attempt_tick = current_tick;
    saved_queue_generation = crazypod_queue_generation();
    saved_queue_hash = queue_hash;
    saved_queue_count = 0;
    saved_queue_snapshot_valid = true;
    state_save_failures = 0;
    state_dirty = false;
    reduce_motion = false;
    global_settings.storage_mode = 0;
    storage_set_storage_mode(global_settings.storage_mode);

    crazypod_apps_reset();
    if(!load_header(&state))
        return;

    reduce_motion = state.reduce_motion != 0;
    clamp_and_apply_settings(&state);
    if(state.menu_count > 0 &&
       state.menu_count <= CRAZYPOD_APP_COUNT)
        crazypod_apps_restore(state.menu_order, state.menu_count,
                              state.menu_enabled_mask);
    crazypod_queue_restore_begin();
    fd = open(QUEUE_PATH, O_RDONLY);
    if(fd >= 0) {
        while(read_line(fd, line, sizeof(line)) >= 0) {
            size_t length = strlen(line);
            if(length == 0 || line[0] != '/' || !file_exists(line))
                continue;
            if(crazypod_queue_restore_add(line)) {
                queue_hash = hash_bytes(queue_hash, line, length);
                queue_hash = hash_bytes(queue_hash, "\n", 1);
                ++queue_count;
            }
        }
        close(fd);
    }

    if(queue_count == state.queue_count && queue_hash == state.queue_hash) {
        crazypod_queue_restore_finish(state.queue_index,
                                      state.shuffled != 0);
        resume_elapsed = state.elapsed;
        last_saved_elapsed = state.elapsed;
    }
    else {
        crazypod_queue_restore_finish(0, false);
    }
    saved_queue_generation = crazypod_queue_generation();
    saved_queue_hash = queue_hash;
    saved_queue_count = queue_count;
    saved_queue_snapshot_valid = true;
}

void crazypod_state_mark_dirty(void)
{
    state_dirty = true;
}

bool crazypod_state_reduce_motion(void)
{
    return reduce_motion;
}

void crazypod_state_set_reduce_motion(bool enabled)
{
    if(reduce_motion == enabled)
        return;
    reduce_motion = enabled;
    state_dirty = true;
}

void crazypod_state_forget_resume(void)
{
    resume_elapsed = 0;
    last_saved_elapsed = 0;
    state_dirty = true;
}

unsigned long crazypod_state_take_resume_elapsed(void)
{
    unsigned long elapsed = resume_elapsed;
    resume_elapsed = 0;
    return elapsed;
}

static bool save_queue(uint32_t *hash_out, uint32_t *count_out)
{
    uint32_t hash = 2166136261u;
    uint32_t count = 0;
    int fd;
    int i;
    bool success = true;

    mkdir(STATE_DIRECTORY);
    fd = open(QUEUE_TEMP_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;

    for(i = 0; i < crazypod_queue_count(); ++i) {
        const char *path = crazypod_queue_path(i);
        size_t length;
        if(path == NULL || path[0] != '/')
            continue;
        length = strlen(path);
        if(!write_exact(fd, path, length) ||
           !write_exact(fd, "\n", 1)) {
            success = false;
            break;
        }
        hash = hash_bytes(hash, path, length);
        hash = hash_bytes(hash, "\n", 1);
        ++count;
    }
    if(fsync(fd) < 0)
        success = false;
    close(fd);
    if(!success || rename(QUEUE_TEMP_PATH, QUEUE_PATH) < 0)
        return false;

    *hash_out = hash;
    *count_out = count;
    return true;
}

void crazypod_state_save(bool force)
{
    struct crazypod_state_disk state;
    struct mp3entry *id3;
    uint32_t queue_hash;
    uint32_t queue_count;
    unsigned long elapsed;
    int fd;
    int i;
    bool success;

    id3 = audio_current_track();
    elapsed = id3 != NULL ? id3->elapsed : resume_elapsed;
    if(!force && !state_dirty &&
       crazypod_queue_generation() == saved_queue_generation &&
       elapsed / 30000 == last_saved_elapsed / 30000) {
        last_save_tick = current_tick;
        return;
    }

    last_save_attempt_tick = current_tick;
    if(saved_queue_snapshot_valid &&
       crazypod_queue_generation() == saved_queue_generation) {
        queue_hash = saved_queue_hash;
        queue_count = saved_queue_count;
    }
    else {
        if(!save_queue(&queue_hash, &queue_count)) {
            state_save_failed();
            return;
        }
        saved_queue_hash = queue_hash;
        saved_queue_count = queue_count;
        saved_queue_generation = crazypod_queue_generation();
        saved_queue_snapshot_valid = true;
    }

    memset(&state, 0, sizeof(state));
    state.magic = STATE_MAGIC;
    state.version = STATE_VERSION;
    state.size = sizeof(state);
    state.volume = global_status.volume;
    state.repeat_mode = crazypod_queue_repeat();
    state.shuffled = crazypod_queue_shuffle();
    state.queue_index = crazypod_queue_index();
    state.queue_count = queue_count;
    state.queue_hash = queue_hash;
    state.elapsed = elapsed;
    state.eq_enabled = global_settings.eq_enabled ? 1 : 0;
    state.bass = global_settings.bass;
    state.treble = global_settings.treble;
    state.balance = global_settings.balance;
#ifdef HAVE_BACKLIGHT_BRIGHTNESS
    state.brightness = global_settings.brightness;
#endif
    state.backlight_timeout = global_settings.backlight_timeout;
#if CONFIG_CHARGING
    state.backlight_timeout_plugged =
        global_settings.backlight_timeout_plugged;
#endif
    state.lcd_sleep_after_backlight_off =
        global_settings.lcd_sleep_after_backlight_off;
    state.sleeptimer_duration = global_settings.sleeptimer_duration;
    state.sleeptimer_on_startup =
        global_settings.sleeptimer_on_startup ? 1 : 0;
    state.keypress_restarts_sleeptimer =
        global_settings.keypress_restarts_sleeptimer ? 1 : 0;
#ifdef HAVE_USB_CHARGING_ENABLE
    state.usb_charging = global_settings.usb_charging;
#endif
    state.beep = global_settings.beep;
    state.keyclick = global_settings.keyclick;
    state.keyclick_repeats = global_settings.keyclick_repeats ? 1 : 0;
#ifdef HAVE_HARDWARE_CLICK
    state.keyclick_hardware = global_settings.keyclick_hardware ? 1 : 0;
#endif
    state.eq_precut = global_settings.eq_precut;
    for(i = 0; i < EQ_NUM_BANDS; ++i) {
        state.eq_bands[i].cutoff =
            global_settings.eq_band_settings[i].cutoff;
        state.eq_bands[i].q = global_settings.eq_band_settings[i].q;
        state.eq_bands[i].gain = global_settings.eq_band_settings[i].gain;
    }
    state.menu_count = CRAZYPOD_APP_COUNT;
    crazypod_apps_export(state.menu_order, sizeof(state.menu_order),
                         &state.menu_enabled_mask);
    state.reduce_motion = reduce_motion ? 1 : 0;
    state.storage_mode = global_settings.storage_mode;
    state.checksum = state_checksum(&state);

    fd = open(STATE_TEMP_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0) {
        state_save_failed();
        return;
    }
    success = write_exact(fd, &state, sizeof(state));
    if(fsync(fd) < 0)
        success = false;
    close(fd);
    if(!success || rename(STATE_TEMP_PATH, STATE_PATH) < 0) {
        state_save_failed();
        return;
    }

    state_save_failures = 0;
    state_dirty = false;
    last_saved_elapsed = elapsed;
    last_save_tick = current_tick;
    saved_queue_generation = crazypod_queue_generation();
    saved_queue_hash = queue_hash;
    saved_queue_count = queue_count;
    saved_queue_snapshot_valid = true;
}

void crazypod_state_tick(void)
{
    if(TIME_AFTER(current_tick, last_save_tick + STATE_SAVE_INTERVAL) &&
       TIME_AFTER(current_tick,
                  last_save_attempt_tick +
                      state_save_retry_interval())) {
        /*
         * Do not wake a sleeping disk solely to advance the elapsed-time
         * crash-recovery checkpoint. Dirty settings and queue changes still
         * save immediately; a later audio refill or forced shutdown save
         * persists the newest elapsed value.
         */
        if(!state_dirty &&
           crazypod_queue_generation() == saved_queue_generation &&
           !storage_disk_is_active()) {
            last_save_attempt_tick = current_tick;
            return;
        }
        crazypod_state_save(false);
    }
}

#endif
