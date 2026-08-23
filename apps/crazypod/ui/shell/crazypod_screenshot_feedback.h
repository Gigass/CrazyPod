#ifndef CRAZYPOD_SCREENSHOT_FEEDBACK_H
#define CRAZYPOD_SCREENSHOT_FEEDBACK_H

#include <stdbool.h>

#include "../../crazypod_screen_recording.h"

void crazypod_screenshot_feedback_show(bool saved);
void crazypod_screenshot_feedback_show_recording(
    enum crazypod_screen_recording_result result);
void crazypod_screenshot_feedback_show_recording_event(
    enum crazypod_screen_recording_event event);

#endif
