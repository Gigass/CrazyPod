#ifndef CRAZYPOD_SCREENSHOT_FEEDBACK_H
#define CRAZYPOD_SCREENSHOT_FEEDBACK_H

#include <stdbool.h>

void crazypod_screenshot_feedback_show(bool saved);
bool crazypod_screenshot_feedback_bounds(
    int *left, int *top, int *right, int *bottom);

#endif
