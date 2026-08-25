#include "config.h"

#if defined(IPOD_6G) && defined(SIMULATOR)

#include <stdlib.h>

#include "kernel.h"

#include "crazypod_app_launcher.h"
#include "crazypod_route_actions.h"
#include "crazypod_scene_transition.h"
#include "crazypod_scene_transition_demo.h"

static bool demo_enabled;
static bool shuffle_demo_enabled;
static int demo_stage;
static long demo_due;

void crazypod_scene_transition_demo_init(void)
{
    demo_enabled =
        getenv("CRAZYPOD_SIM_TRANSITION_DEMO") != NULL;
    shuffle_demo_enabled =
        getenv("CRAZYPOD_SIM_SHUFFLE_DEMO") != NULL;
    demo_stage = -1;
    demo_due = 0;
}

void crazypod_scene_transition_demo_service(
    enum crazypod_screen_recording_event event, long now)
{
    if((demo_enabled || shuffle_demo_enabled) &&
       event == CRAZYPOD_SCREEN_RECORDING_EVENT_STARTED) {
        demo_stage = 0;
        demo_due = now + 3 * HZ / 4;
    }
    if(demo_stage < 0 || TIME_BEFORE(now, demo_due) ||
       crazypod_scene_transition_active())
        return;

    if(shuffle_demo_enabled) {
        crazypod_app_launcher_simulate_shuffle_ready();
        demo_stage = -1;
        return;
    }

    switch(demo_stage) {
    case 0:
        crazypod_app_launcher_open_root(SETTINGS_ROUTE_MENU);
        break;
    case 1:
        crazypod_route_actions_push(SETTINGS_ROUTE_SOUND, -1);
        break;
    case 2:
        crazypod_route_actions_pop();
        break;
    case 3:
        crazypod_app_launcher_open_root(EXTRAS_ROUTE_MENU);
        break;
    case 4:
        crazypod_route_actions_pop();
        break;
    default:
        demo_stage = -1;
        return;
    }
    ++demo_stage;
    demo_due = now + HZ;
}

#endif
