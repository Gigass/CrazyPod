#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>

#include "crazypod_notification.h"
#include "crazypod_screenshot_feedback.h"
#include "crazypod_status_bar.h"

#define COUNTDOWN_NOTIFICATION_MS 900

void crazypod_screenshot_feedback_show(bool saved)
{
    crazypod_notification_show(
        saved ? CRAZYPOD_NOTIFICATION_SUCCESS
              : CRAZYPOD_NOTIFICATION_ERROR,
        saved ? CP_TR("Saved to Photos")
              : CP_TR("Screenshot failed"));
    if(saved)
        crazypod_notification_flash();
}

void crazypod_screenshot_feedback_show_recording(
    enum crazypod_screen_recording_result result)
{
    bool failed = result == CRAZYPOD_SCREEN_RECORDING_FAILED;

    if(result == CRAZYPOD_SCREEN_RECORDING_COUNTDOWN_STARTED) {
        char message[64];

        snprintf(
            message, sizeof(message),
            CP_FMT("Screen recording starts in %d"), 3);
        crazypod_notification_show_for(
            CRAZYPOD_NOTIFICATION_INFO, message,
            COUNTDOWN_NOTIFICATION_MS);
        return;
    }
    crazypod_status_bars_update();
    crazypod_notification_show(
        failed ? CRAZYPOD_NOTIFICATION_ERROR
               : CRAZYPOD_NOTIFICATION_SUCCESS,
        failed ? CP_TR("Screen recording failed")
               : result == CRAZYPOD_SCREEN_RECORDING_CANCELLED
                   ? CP_TR("Screen recording cancelled")
               : result == CRAZYPOD_SCREEN_RECORDING_STARTED
                   ? CP_TR("Screen recording started")
                   : CP_TR("Screen recording ended"));
    if(!failed)
        crazypod_notification_flash();
}

void crazypod_screenshot_feedback_show_recording_event(
    enum crazypod_screen_recording_event event)
{
    int countdown;

    if(event == CRAZYPOD_SCREEN_RECORDING_EVENT_NONE)
        return;
    countdown = event == CRAZYPOD_SCREEN_RECORDING_EVENT_COUNTDOWN_2
        ? 2
        : event == CRAZYPOD_SCREEN_RECORDING_EVENT_COUNTDOWN_1
            ? 1 : 0;
    if(countdown > 0) {
        char message[64];

        snprintf(
            message, sizeof(message),
            CP_FMT("Screen recording starts in %d"), countdown);
        crazypod_notification_show_for(
            CRAZYPOD_NOTIFICATION_INFO, message,
            COUNTDOWN_NOTIFICATION_MS);
        return;
    }
    if(event == CRAZYPOD_SCREEN_RECORDING_EVENT_SAVED) {
        crazypod_status_bars_update();
        crazypod_notification_show(
            CRAZYPOD_NOTIFICATION_SUCCESS,
            CP_TR("Saved to Videos"));
        crazypod_notification_flash();
        return;
    }
    crazypod_screenshot_feedback_show_recording(
        event == CRAZYPOD_SCREEN_RECORDING_EVENT_FAILED
            ? CRAZYPOD_SCREEN_RECORDING_FAILED
            : event == CRAZYPOD_SCREEN_RECORDING_EVENT_STARTED
                ? CRAZYPOD_SCREEN_RECORDING_STARTED
            : CRAZYPOD_SCREEN_RECORDING_STOPPED);
}

#endif
