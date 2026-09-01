#include <assert.h>

#include "crazypod_screen_recording.h"

int main(void)
{
    bool suppressed = true;

    assert(crazypod_screen_recording_completion_event(false) ==
           CRAZYPOD_SCREEN_RECORDING_EVENT_SAVED);
    assert(crazypod_screen_recording_completion_event(true) ==
           CRAZYPOD_SCREEN_RECORDING_EVENT_FAILED);
    assert(crazypod_screen_recording_limit_event(true) ==
           CRAZYPOD_SCREEN_RECORDING_EVENT_SAVED);
    assert(crazypod_screen_recording_limit_event(false) ==
           CRAZYPOD_SCREEN_RECORDING_EVENT_FAILED);
    assert(crazypod_screen_recording_claim_backlight_restore(
               &suppressed));
    assert(!suppressed);
    assert(!crazypod_screen_recording_claim_backlight_restore(
        &suppressed));
    return 0;
}
