#ifndef CRAZYPOD_SCREEN_RECORDING_H
#define CRAZYPOD_SCREEN_RECORDING_H

#include <stdbool.h>
#include <stdint.h>

enum crazypod_screen_recording_result {
    CRAZYPOD_SCREEN_RECORDING_STARTED = 1,
    CRAZYPOD_SCREEN_RECORDING_STOPPED = 2,
    CRAZYPOD_SCREEN_RECORDING_COUNTDOWN_STARTED = 3,
    CRAZYPOD_SCREEN_RECORDING_CANCELLED = 4,
    CRAZYPOD_SCREEN_RECORDING_FAILED = -1,
};

enum crazypod_screen_recording_event {
    CRAZYPOD_SCREEN_RECORDING_EVENT_NONE = 0,
    CRAZYPOD_SCREEN_RECORDING_EVENT_COUNTDOWN_2,
    CRAZYPOD_SCREEN_RECORDING_EVENT_COUNTDOWN_1,
    CRAZYPOD_SCREEN_RECORDING_EVENT_STARTED,
    CRAZYPOD_SCREEN_RECORDING_EVENT_SAVED,
    CRAZYPOD_SCREEN_RECORDING_EVENT_FAILED,
};

static inline enum crazypod_screen_recording_event
crazypod_screen_recording_completion_event(bool failed)
{
    return failed
        ? CRAZYPOD_SCREEN_RECORDING_EVENT_FAILED
        : CRAZYPOD_SCREEN_RECORDING_EVENT_SAVED;
}

static inline enum crazypod_screen_recording_event
crazypod_screen_recording_limit_event(bool stop_succeeded)
{
    return stop_succeeded
        ? CRAZYPOD_SCREEN_RECORDING_EVENT_SAVED
        : CRAZYPOD_SCREEN_RECORDING_EVENT_FAILED;
}

static inline bool crazypod_screen_recording_claim_backlight_restore(
    bool *suppressed)
{
    if(!suppressed || !*suppressed)
        return false;
    *suppressed = false;
    return true;
}

void crazypod_screen_recording_init(void);
enum crazypod_screen_recording_result
crazypod_screen_recording_toggle(long now);
bool crazypod_screen_recording_stop(long now);
enum crazypod_screen_recording_event
crazypod_screen_recording_service(long now);
int crazypod_screen_recording_wait_ticks(long now);
bool crazypod_screen_recording_active(void);
uint32_t crazypod_screen_recording_dropped_frames(void);

#endif
