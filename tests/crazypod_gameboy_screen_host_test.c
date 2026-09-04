#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio.h"
#include "metadata.h"
#include "button.h"
#include "kernel.h"
#include "pcm_mixer.h"
#include "system.h"
#include "usb.h"
#include "ui/features/miniapps/crazypod_gameboy_screen.h"

enum scenario { USB, HOLD, RETRY_SAVE, BAD_ROM, CORE_ERROR, POWER, REBOOT };
static enum scenario scenario;
long current_tick;
static long posted_event;
static intptr_t posted_data;
static unsigned frames, saves, closed, restored, frequency, locks;
static unsigned suspension, resumes, boosts;
static unsigned received_buttons;
static bool opened, reserve_released;
static uint16_t framebuffer[320 * 240], pixels[160 * 144];
static void (*submit_audio)(const int16_t *, size_t);
static pcm_play_callback_type pcm_callback;

long button_get(bool block)
{
    long tick = current_tick;
    (void)block;
    assert(tick < 150); /* catches failure to leave any scenario */
    if(tick == 3 || tick == 117 || tick == 120)
        return BUTTON_SELECT;
    if(tick == 4 || tick == 118 || tick == 121)
        return BUTTON_SELECT | BUTTON_REL;
    if(scenario == RETRY_SAVE) {
        if(tick == 111)
            return BUTTON_SELECT | BUTTON_REL;
        if(tick == 115)
            return BUTTON_SCROLL_BACK;
    }
    if(tick == 20) {
        if(scenario == USB) return SYS_USB_CONNECTED;
        if(scenario == POWER) return SYS_POWEROFF;
        if(scenario == REBOOT) return SYS_REBOOT;
    }
    return BUTTON_NONE;
}

int button_status(void)
{
    return current_tick == 3 || current_tick == 117 || current_tick == 120 ||
        (scenario == RETRY_SAVE && current_tick >= 10 && current_tick <= 110)
        ? BUTTON_SELECT : 0;
}
intptr_t button_get_data(void) { return 0x1234; }
void button_queue_post(long event, intptr_t data)
{
    posted_event = event; posted_data = data;
}
bool button_hold(void) { return scenario == HOLD && current_tick >= 20; }
int wheel_status(void) { return current_tick >= 5 ? 0 : -1; }
void sleep(int ticks) { current_tick += ticks; }
void yield(void)
{
    if(pcm_callback) {
        const void *next;
        size_t size;
        pcm_callback(&next, &size);
    }
    ++current_tick;
}
void pcm_play_lock(void) { ++locks; }
void pcm_play_unlock(void) { assert(locks > 0); --locks; }
void mixer_channel_play_data(
    int channel, const struct mixer_play_cbs *callbacks,
                             const void *data, size_t size)
{
    assert(channel == 0 && locks > 0 && data != NULL && size > 0);
    pcm_callback = callbacks != NULL ? callbacks->get_more : NULL;
}
void mixer_channel_stop(int channel) { assert(channel == 0); pcm_callback = NULL; }
void mixer_channel_set_amplitude(int channel, unsigned amplitude)
{ assert(channel == 0 && amplitude == MIX_AMP_UNITY); }
void mixer_set_frequency(unsigned value) { frequency = value; }
unsigned mixer_get_frequency(void) { return frequency; }
int audio_status(void) { return AUDIO_STATUS_PLAY | AUDIO_STATUS_PAUSE; }
struct mp3entry *audio_current_track(void)
{ static struct mp3entry track = { 123, 456 }; return &track; }
void audio_hard_stop(void) { }
void audio_play(unsigned long elapsed, unsigned long offset)
{ assert(elapsed == 123 && offset == 456); ++restored; }
void audio_pause(void) { ++restored; }
void backlight_on(void) { }
void reset_poweroff_timer(void) { }
void lcd_update(void) { }
void *crazypod_platform_display_framebuffer(void) { return framebuffer; }
void crazypod_lcd_draw_text(const char *text, int x, int y,
                            int maximum_x, uint32_t color)
{ (void)text; (void)x; (void)y; (void)maximum_x; (void)color; }
void panicf(const char *format, ...) { (void)format; abort(); }
void cpu_boost(bool value) { boosts += value ? 1 : 10; }
bool crazypod_screen_recording_stop(long now) { (void)now; return true; }
void crazypod_music_set_scan_suspended(bool value)
{ if(value) ++suspension; else ++resumes; }
void crazypod_artwork_suspend(void) { ++suspension; }
void crazypod_photos_suspend(void) { ++suspension; }
void crazypod_videos_suspend(void) { ++suspension; }
void crazypod_artwork_resume(void) { ++resumes; }
void crazypod_photos_resume(void) { ++resumes; }
void crazypod_videos_resume(void) { ++resumes; }
void crazypod_audio_reserve_release(void) { reserve_released = true; }
bool crazypod_audio_reserve_acquire(void)
{ assert(!opened); reserve_released = false; return true; }
enum crazypod_gameboy_result crazypod_gameboy_open(
    int index, void (*audio)(const int16_t *, size_t))
{
    assert(index == 0 && reserve_released && suspension == 4);
    if(scenario == BAD_ROM)
        return CRAZYPOD_GAMEBOY_BAD_ROM;
    submit_audio = audio;
    opened = true;
    return CRAZYPOD_GAMEBOY_OK;
}
bool crazypod_gameboy_core_frame(uint8_t buttons, bool render)
{
    static int16_t samples[2048];
    assert(opened);
    (void)render;
    ++frames;
    received_buttons |= buttons;
    submit_audio(samples, 2048);
    return scenario != CORE_ERROR;
}
const uint16_t *crazypod_gameboy_core_pixels(void) { return pixels; }
void crazypod_gameboy_core_clock_advance(uint32_t seconds) { (void)seconds; }
bool crazypod_gameboy_save(void)
{
    assert(opened);
    ++saves;
    return scenario != RETRY_SAVE || saves > 1;
}
void crazypod_gameboy_close(void) { opened = false; ++closed; }

int main(void)
{
    for(scenario = USB; scenario <= REBOOT; ++scenario) {
        enum crazypod_gameboy_result result;
        current_tick = 1;
        frames = saves = closed = restored = suspension = resumes = boosts = 0;
        received_buttons = 0;
        frequency = 48000;
        posted_event = posted_data = 0;
        result = crazypod_gameboy_screen_run(0);
        assert(closed == 1 && !opened && !reserve_released);
        assert(resumes == suspension && boosts == 11 && locks == 0);
        assert(frequency == 48000 && pcm_callback == NULL);
        if(scenario == BAD_ROM) {
            assert(result == CRAZYPOD_GAMEBOY_BAD_ROM);
            assert(frames == 0 && saves == 0 && restored == 2);
        }
        else {
            assert(frames > 0 && restored == 0);
            assert(received_buttons & CRAZYPOD_GB_UP); /* wheel at zero */
            assert(saves == (scenario == RETRY_SAVE ? 2u : 1u));
            assert(result == (scenario == CORE_ERROR ?
                CRAZYPOD_GAMEBOY_CORE_ERROR : CRAZYPOD_GAMEBOY_OK));
        }
        if(scenario == USB || scenario == POWER || scenario == REBOOT) {
            assert(posted_event == (scenario == USB ? SYS_USB_CONNECTED :
                scenario == POWER ? SYS_POWEROFF : SYS_REBOOT));
            assert(posted_data == 0x1234);
        }
        else
            assert(posted_event == 0);
    }
    puts("Game Boy screen: USB/power/HOLD, pause/save retry, cleanup pass");
    return 0;
}
