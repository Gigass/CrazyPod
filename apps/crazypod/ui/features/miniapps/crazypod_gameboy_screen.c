#include "config.h"
#include <string.h>
#include "audio.h"
#include "backlight.h"
#include "button.h"
#include "kernel.h"
#include "lcd.h"
#include "metadata.h"
#include "panic.h"
#include "pcm.h"
#include "pcm_mixer.h"
#include "powermgmt.h"
#include "system.h"
#include "usb.h"
#include "../../../crazypod_artwork.h"
#include "../../../crazypod_audio_reserve.h"
#include "../../../crazypod_l10n.h"
#include "../../../crazypod_lcd.h"
#include "../../../crazypod_music.h"
#include "../../../crazypod_photos.h"
#include "../../../crazypod_screen_recording.h"
#include "../../../crazypod_videos.h"
#include "../../../platform/crazypod_platform_display.h"
#include "crazypod_gameboy_screen.h"

#define AUDIO_SLOTS 8
#define AUDIO_SAMPLES 2048

/* The head remains owned by the mixer until the next callback. */
static int16_t audio_ring[AUDIO_SLOTS][AUDIO_SAMPLES];
static size_t audio_sizes[AUDIO_SLOTS];
static unsigned audio_head, audio_count;
static bool audio_playing;

static void game_audio_next(const void **start, size_t *size)
{
    if(audio_count > 0) {
        audio_head = (audio_head + 1) % AUDIO_SLOTS;
        --audio_count;
    }
    if(audio_count > 0) {
        *start = audio_ring[audio_head];
        *size = audio_sizes[audio_head];
    }
    else {
        *start = NULL;
        *size = 0;
        audio_playing = false;
    }
}

static void audio_submit(const int16_t *samples, size_t count)
{
    unsigned tail;

    if(count == 0 || count > AUDIO_SAMPLES)
        return;
    pcm_play_lock();
    if(audio_count < AUDIO_SLOTS) {
        tail = (audio_head + audio_count) % AUDIO_SLOTS;
        memcpy(audio_ring[tail], samples, count * sizeof(*samples));
        audio_sizes[tail] = count * sizeof(*samples);
        ++audio_count;
    }
    if(!audio_playing && audio_count >= 2) {
        audio_playing = true;
        mixer_channel_play_data(PCM_MIXER_CHAN_PLAYBACK, game_audio_next,
                                audio_ring[audio_head],
                                audio_sizes[audio_head]);
    }
    pcm_play_unlock();
}

static void game_audio_stop(void)
{
    pcm_play_lock();
    mixer_channel_stop(PCM_MIXER_CHAN_PLAYBACK);
    audio_count = audio_head = 0;
    audio_playing = false;
    pcm_play_unlock();
}

static void draw_frame(void)
{
    const uint16_t *pixels = crazypod_gameboy_core_pixels();
    fb_data *target = crazypod_platform_display_framebuffer();
    int x, y;

    /* 1.5x nearest-neighbour, preserving the 10:9 game aspect ratio. */
    memset(target, 0, LCD_WIDTH * LCD_HEIGHT * sizeof(*target));
    for(y = 0; y < 216; ++y)
        for(x = 0; x < 240; ++x)
            target[(y + 12) * LCD_WIDTH + x + 40] =
                pixels[(y * 2 / 3) * 160 + x * 2 / 3];
    lcd_update();
}

static void draw_menu(int selected, bool save_failed)
{
    static const char *const items[] = {
        CP_TR("Resume"), "START", "SELECT", CP_TR("Save and exit")
    };
    fb_data *target = crazypod_platform_display_framebuffer();
    int i;

    memset(target, 0, LCD_WIDTH * LCD_HEIGHT * sizeof(*target));
    crazypod_lcd_draw_text(CP_TR("Game Boy"), 18, 16, 304, 0xffffff);
    for(i = 0; i < 4; ++i) {
        if(i == selected)
            crazypod_lcd_draw_text(">", 18, 54 + 28 * i, 34, 0x69bfff);
        crazypod_lcd_draw_text(items[i], 40, 54 + 28 * i, 306,
                                i == selected ? 0x69bfff : 0xffffff);
    }
    crazypod_lcd_draw_text(save_failed ? CP_TR("Game save failed") :
        CP_TR("Hold Center for game menu"), 18, 190, 306, 0xffd477);
    crazypod_lcd_draw_text(
        CP_TR("Controls: /MiniApps/Games/README.txt"),
        18, 212, 306, 0xaaaaaa);
    lcd_update();
}

static uint8_t game_buttons(int physical, long b_until)
{
    uint8_t result = 0;

#if defined(HAVE_WHEEL_POSITION) && !defined(SIMULATOR)
    static const uint8_t sectors[8] = {
        CRAZYPOD_GB_UP, CRAZYPOD_GB_A, CRAZYPOD_GB_RIGHT,
        CRAZYPOD_GB_START, CRAZYPOD_GB_DOWN, CRAZYPOD_GB_SELECT,
        CRAZYPOD_GB_LEFT, CRAZYPOD_GB_B
    };
    int position = wheel_status();

    (void)b_until;
    if(position >= 0 && position < 96)
        result |= sectors[((position + 6) / 12) % 8];
    if(physical & (BUTTON_MENU | BUTTON_PLAY | BUTTON_LEFT | BUTTON_RIGHT))
        result |= CRAZYPOD_GB_B;
#else
    if(physical & BUTTON_MENU) result |= CRAZYPOD_GB_UP;
    if(physical & BUTTON_PLAY) result |= CRAZYPOD_GB_DOWN;
    if(physical & BUTTON_LEFT) result |= CRAZYPOD_GB_LEFT;
    if(physical & BUTTON_RIGHT) result |= CRAZYPOD_GB_RIGHT;
    if(TIME_BEFORE(current_tick, b_until)) result |= CRAZYPOD_GB_B;
#endif
    if(physical & BUTTON_SELECT)
        result |= CRAZYPOD_GB_A;
    return result;
}

const char *crazypod_gameboy_screen_error(
    enum crazypod_gameboy_result result)
{
    switch(result) {
    case CRAZYPOD_GAMEBOY_OK: return "";
    case CRAZYPOD_GAMEBOY_BAD_ROM:
        return CP_TR("Unsupported or damaged Game Boy ROM");
    case CRAZYPOD_GAMEBOY_NO_MEMORY:
        return CP_TR("Not enough memory for this game");
    case CRAZYPOD_GAMEBOY_BAD_SAVE:
        return CP_TR("Game save is damaged; original preserved");
    case CRAZYPOD_GAMEBOY_CORE_ERROR:
        return CP_TR("Game emulation stopped");
    default: return CP_TR("Game file or save could not be written/read");
    }
}

enum crazypod_gameboy_result crazypod_gameboy_screen_run(int index)
{
    enum crazypod_gameboy_result result;
    unsigned old_frequency = mixer_get_frequency();
    long system_event = 0, deadline, hold_since = 0, pause_tick;
    intptr_t system_data = 0;
    unsigned fraction = 0, frames = 0, pulse_frames = 0;
    uint8_t pulse = 0;
    long b_until = current_tick;
    int selected = 0;
    bool paused = true, wait_release = true, save_failed = false;
    bool saved = false;
    int old_audio_state = audio_status();
    struct mp3entry *track = audio_current_track();
    unsigned long old_elapsed = track != NULL ? track->elapsed : 0;
    unsigned long old_offset = track != NULL ? track->offset : 0;
    bool started = false;

    (void)crazypod_screen_recording_stop(current_tick);
    crazypod_music_set_scan_suspended(true);
    crazypod_artwork_suspend();
    crazypod_photos_suspend();
    crazypod_videos_suspend();
    audio_hard_stop();
    crazypod_audio_reserve_release();
    cpu_boost(true);
    result = crazypod_gameboy_open(index, audio_submit);
    if(result != CRAZYPOD_GAMEBOY_OK)
        goto cleanup;
    started = true;
    mixer_set_frequency(44100);
    mixer_channel_set_amplitude(PCM_MIXER_CHAN_PLAYBACK, MIX_AMP_UNITY);
    deadline = pause_tick = current_tick;
    draw_menu(selected, false);

    for(;;) {
        long event = button_get(false);
        int physical = button_status();
        int base = event & ~(BUTTON_REL | BUTTON_REPEAT);
        bool center_released = base == BUTTON_SELECT &&
            (event & BUTTON_REL);

        if(event == SYS_USB_CONNECTED || event == SYS_POWEROFF ||
           event == SYS_REBOOT) {
            system_event = event;
            system_data = button_get_data();
            break;
        }
#ifdef HAS_BUTTON_HOLD
        if(button_hold())
            break;
#endif
        reset_poweroff_timer();
        backlight_on();
        if(wait_release) {
            if(!(physical & BUTTON_SELECT))
                wait_release = false;
            center_released = false;
        }
        if(paused) {
            if(!(event & BUTTON_REL) &&
               (base == BUTTON_SCROLL_FWD || base == BUTTON_PLAY)) {
                selected = (selected + 1) % 4;
                draw_menu(selected, save_failed);
            }
            else if(!(event & BUTTON_REL) &&
                    (base == BUTTON_SCROLL_BACK || base == BUTTON_MENU)) {
                selected = (selected + 3) % 4;
                draw_menu(selected, save_failed);
            }
            if(center_released) {
                if(selected == 3) {
                    crazypod_gameboy_core_clock_advance(
                        (uint32_t)(current_tick - pause_tick) / HZ);
                    pause_tick = current_tick;
                    if(crazypod_gameboy_save()) {
                        saved = true;
                        break;
                    }
                    save_failed = true;
                    draw_menu(selected, true);
                }
                else {
                    pulse = selected == 1 ? CRAZYPOD_GB_START :
                        selected == 2 ? CRAZYPOD_GB_SELECT : 0;
                    pulse_frames = 4;
                    crazypod_gameboy_core_clock_advance(
                        (uint32_t)(current_tick - pause_tick) / HZ);
                    paused = false;
                    hold_since = 0;
                    deadline = current_tick;
                    draw_frame();
                }
            }
            sleep(1);
            continue;
        }
        if(physical & BUTTON_SELECT) {
            if(hold_since == 0)
                hold_since = current_tick;
            else if(!TIME_BEFORE(current_tick, hold_since + HZ)) {
                paused = true;
                pause_tick = current_tick;
                wait_release = true;
                selected = 0;
                game_audio_stop();
                draw_menu(selected, save_failed);
                continue;
            }
        }
        else
            hold_since = 0;
        if(base == BUTTON_SCROLL_FWD || base == BUTTON_SCROLL_BACK)
            b_until = current_tick + HZ / 10;
        if(TIME_BEFORE(current_tick, deadline)) {
            sleep(1);
            continue;
        }
        if(!crazypod_gameboy_core_frame(
               game_buttons(physical, b_until) |
                   (pulse_frames > 0 ? pulse : 0), (frames & 1) == 0)) {
            result = CRAZYPOD_GAMEBOY_CORE_ERROR;
            break;
        }
        if(pulse_frames > 0)
            --pulse_frames;
        if((frames++ & 1) == 0)
            draw_frame();
        /* 70224 cycles / 4194304 Hz; cap catch-up after slow storage/UI. */
        fraction += HZ * 4389u;
        deadline += fraction / 262144u;
        fraction %= 262144u;
        if(TIME_AFTER(current_tick, deadline + HZ / 4))
            deadline = current_tick;
        yield();
    }
    game_audio_stop();
    if(paused && !saved)
        crazypod_gameboy_core_clock_advance(
            (uint32_t)(current_tick - pause_tick) / HZ);
    if(!saved && !crazypod_gameboy_save())
        result = CRAZYPOD_GAMEBOY_IO_ERROR;

cleanup:
    game_audio_stop();
    crazypod_gameboy_close();
    mixer_set_frequency(old_frequency);
    if(!crazypod_audio_reserve_acquire())
        panicf("audio reserve after gameboy");
    if(!started && (old_audio_state & AUDIO_STATUS_PLAY)) {
        audio_play(old_elapsed, old_offset);
        if(old_audio_state & AUDIO_STATUS_PAUSE)
            audio_pause();
    }
    cpu_boost(false);
    crazypod_videos_resume();
    crazypod_photos_resume();
    crazypod_artwork_resume();
    crazypod_music_set_scan_suspended(false);
    /* Keep any queued USB/power events; only the consumed one is reposted. */
    if(system_event != 0)
        button_queue_post(system_event, system_data);
    return result;
}
