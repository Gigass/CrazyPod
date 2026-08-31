#include "config.h"

#ifdef IPOD_6G

#include <limits.h>

#include "audio.h"
#include "crazypod_usb_iap.h"
#include "iap-usb.h"
#include "settings.h"
#include "sound.h"

#include "../crazypod_playlist.h"
#include "../crazypod_state.h"
#include "../ui/app/crazypod_playback.h"

bool crazypod_usb_iap_control(enum crazypod_usb_iap_control control)
{
    if(!crazypod_playback_commands_ready())
        return false;

    switch(control) {
    case CRAZYPOD_USB_IAP_TOGGLE_PLAY_PAUSE:
        if(audio_status() == 0) {
            int index = crazypod_queue_index();

            if(index < 0 || index >= crazypod_queue_count())
                return false;
            crazypod_playback_select_async(index);
            return true;
        }
        crazypod_playback_toggle_async();
        return true;
    case CRAZYPOD_USB_IAP_STOP:
        crazypod_playback_stop_async();
        return true;
    case CRAZYPOD_USB_IAP_NEXT:
        crazypod_playback_next_async();
        return true;
    case CRAZYPOD_USB_IAP_PREVIOUS:
        crazypod_playback_previous_or_restart_async();
        return true;
    }

    return false;
}

void crazypod_usb_iap_adjust_volume(int direction)
{
    int volume = global_status.volume + direction;

    if(volume < sound_min(SOUND_VOLUME))
        volume = sound_min(SOUND_VOLUME);
    if(volume > sound_max(SOUND_VOLUME))
        volume = sound_max(SOUND_VOLUME);
    if(volume == global_status.volume)
        return;
    sound_set_volume(volume);
    global_status.volume = volume;
    iap_on_volume(volume);
    crazypod_state_mark_dirty();
}

bool crazypod_usb_iap_shuffle(void)
{
    return crazypod_queue_shuffle();
}

void crazypod_usb_iap_set_shuffle(bool enabled)
{
    if(crazypod_queue_shuffle() == enabled)
        return;
    crazypod_queue_set_shuffle(enabled);
    crazypod_state_mark_dirty();
}

int crazypod_usb_iap_repeat(void)
{
    return crazypod_queue_repeat();
}

void crazypod_usb_iap_set_repeat(int repeat_mode)
{
    if(crazypod_queue_repeat() == repeat_mode)
        return;
    crazypod_queue_set_repeat(repeat_mode);
    crazypod_state_mark_dirty();
}

bool crazypod_usb_iap_copy_track_path(uint32_t index, char *buffer,
                                      size_t buffer_size)
{
    return index <= INT_MAX &&
        crazypod_queue_copy_path((int)index, buffer, buffer_size);
}

bool crazypod_usb_iap_select_track(uint32_t index)
{
    if(!crazypod_playback_commands_ready() || index > INT_MAX ||
       (int)index >= crazypod_queue_count())
        return false;
    crazypod_playback_select_async((int)index);
    return true;
}

#endif
