#include "config.h"

#ifdef IPOD_6G

#include "../../../crazypod_miniapps.h"
#include "crazypod_miniapp_activation.h"
#include "crazypod_miniapp_runtime_controller.h"
#include "crazypod_miniapp_screen.h"

struct crazypod_miniapp_activation_result
crazypod_miniapp_activation_execute(
    enum crazypod_route route, int selected)
{
    struct crazypod_miniapp_activation_result result = { 0 };

    if(route == MINIAPP_ROUTE_VIEW) {
        result.handled = true;
        return result;
    }
    if(route != UTILITIES_ROUTE_MENU)
        return result;

    result.handled = true;
    result.selected = selected;
    crazypod_miniapp_screen_reset();
    result.error = crazypod_miniapps_open(selected);
    result.opened = result.error == CRAZYPOD_MINIAPP_OK;
    if(result.opened)
        crazypod_miniapp_runtime_opened();
    return result;
}

#endif
