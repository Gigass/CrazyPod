#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef IPOD_6G

#include "crazypod_notification.h"
#include "crazypod_screenshot_feedback.h"

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

#endif
