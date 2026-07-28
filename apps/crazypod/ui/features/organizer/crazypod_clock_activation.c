#include "config.h"

#ifdef IPOD_6G

#include "audio.h"
#include "powermgmt.h"
#include "settings.h"

#include "../../../crazypod_state.h"
#include "crazypod_clock_activation.h"

static struct crazypod_clock_activation_result result(
    enum crazypod_clock_activation_kind kind,
    enum crazypod_route route)
{
    const struct crazypod_clock_activation_result value = {
        .kind = kind,
        .route = route,
    };

    return value;
}

struct crazypod_clock_activation_result
crazypod_clock_activation_execute(struct route_state *state)
{
    if(state->route == CLOCK_ROUTE_MENU) {
        return result(
            CRAZYPOD_CLOCK_ACTIVATION_PUSH,
            state->selected == 0 ? CLOCK_ROUTE_VIEW :
            state->selected == 1 ? CLOCK_ROUTE_SLEEP_TIMER :
                                   STOPWATCH_ROUTE_VIEW);
    }
    if(state->route == CLOCK_ROUTE_SLEEP_TIMER) {
        if(get_sleep_timer_active()) {
            if(state->selected == 1 &&
               (audio_status() & AUDIO_STATUS_PLAY))
                audio_pause();
            set_sleeptimer_duration(0);
        }
        else {
            static const int durations[] = { 15, 30, 45, 60 };

            if(state->selected >= 0 && state->selected < 4) {
                global_settings.sleeptimer_duration =
                    durations[state->selected];
                set_sleeptimer_duration(durations[state->selected]);
                crazypod_state_mark_dirty();
            }
        }
        state->selected = 0;
        return result(
            CRAZYPOD_CLOCK_ACTIVATION_RENDER, state->route);
    }
    if(state->route == CLOCK_ROUTE_VIEW)
        return result(
            CRAZYPOD_CLOCK_ACTIVATION_NONE, state->route);
    return result(
        CRAZYPOD_CLOCK_ACTIVATION_UNHANDLED, state->route);
}

#endif
