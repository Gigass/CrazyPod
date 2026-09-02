#include <assert.h>

#include "button.h"
#include "navigation/crazypod_remote_multitap.h"

static void press(
    struct crazypod_remote_multitap_state *state, long now)
{
    assert(crazypod_remote_multitap_handle_down(
               state, BUTTON_RC_VOL_DOWN, now, 30) ==
           CRAZYPOD_REMOTE_MULTITAP_NONE);
}

static enum crazypod_remote_multitap_action release(
    struct crazypod_remote_multitap_state *state, long now)
{
    return crazypod_remote_multitap_handle_down(
        state, BUTTON_RC_VOL_DOWN | BUTTON_REL, now, 30);
}

int main(void)
{
    struct crazypod_remote_multitap_state state;

    assert(crazypod_remote_multitap_is_down(BUTTON_RC_VOL_DOWN));
    assert(crazypod_remote_multitap_is_down(BUTTON_RC_DOWN));
    assert(!crazypod_remote_multitap_is_down(BUTTON_RC_VOL_UP));

    crazypod_remote_multitap_reset(&state);
    press(&state, 0);
    assert(release(&state, 5) == CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_remote_multitap_wait_ticks(&state, 5, 100) == 30);
    assert(crazypod_remote_multitap_tick(&state, 34) ==
           CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_remote_multitap_tick(&state, 35) ==
           CRAZYPOD_REMOTE_MULTITAP_PLAY_PAUSE);

    crazypod_remote_multitap_reset(&state);
    press(&state, 100);
    assert(release(&state, 105) == CRAZYPOD_REMOTE_MULTITAP_NONE);
    press(&state, 120);
    assert(release(&state, 125) == CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_remote_multitap_tick(&state, 155) ==
           CRAZYPOD_REMOTE_MULTITAP_NEXT);

    crazypod_remote_multitap_reset(&state);
    press(&state, 200);
    assert(release(&state, 205) == CRAZYPOD_REMOTE_MULTITAP_NONE);
    press(&state, 215);
    assert(release(&state, 220) == CRAZYPOD_REMOTE_MULTITAP_NONE);
    press(&state, 230);
    assert(release(&state, 235) ==
           CRAZYPOD_REMOTE_MULTITAP_PREVIOUS);
    assert(crazypod_remote_multitap_tick(&state, 300) ==
           CRAZYPOD_REMOTE_MULTITAP_NONE);

    crazypod_remote_multitap_reset(&state);
    press(&state, 400);
    assert(crazypod_remote_multitap_handle_down(
               &state, BUTTON_RC_VOL_DOWN | BUTTON_REPEAT,
               410, 30) == CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(release(&state, 420) == CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_remote_multitap_tick(&state, 500) ==
           CRAZYPOD_REMOTE_MULTITAP_NONE);

    crazypod_remote_multitap_reset(&state);
    press(&state, 600);
    assert(release(&state, 605) == CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_remote_multitap_flush(&state) ==
           CRAZYPOD_REMOTE_MULTITAP_PLAY_PAUSE);

    crazypod_remote_multitap_reset(&state);
    press(&state, 700);
    assert(release(&state, 705) == CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_remote_multitap_handle_down(
               &state, BUTTON_RC_VOL_DOWN, 740, 30) ==
           CRAZYPOD_REMOTE_MULTITAP_PLAY_PAUSE);
    assert(release(&state, 745) == CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_remote_multitap_tick(&state, 775) ==
           CRAZYPOD_REMOTE_MULTITAP_PLAY_PAUSE);

    crazypod_remote_multitap_reset(&state);
    assert(crazypod_multitap_handle_button(
               &state, BUTTON_SELECT, BUTTON_SELECT,
               800, 30) == CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_multitap_handle_button(
               &state, BUTTON_SELECT | BUTTON_REL,
               BUTTON_SELECT, 805, 30) ==
           CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_multitap_handle_button(
               &state, BUTTON_SELECT, BUTTON_SELECT,
               810, 30) == CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_multitap_handle_button(
               &state, BUTTON_SELECT | BUTTON_REL,
               BUTTON_SELECT, 815, 30) ==
           CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_remote_multitap_tick(&state, 845) ==
           CRAZYPOD_REMOTE_MULTITAP_NEXT);

    /* Mikey emits multimedia press/release pairs. They use the same
     * single/double/triple semantics as the existing remote state machine. */
    crazypod_remote_multitap_reset(&state);
    assert(crazypod_multitap_handle_button(
               &state, BUTTON_MULTIMEDIA_PLAYPAUSE,
               BUTTON_MULTIMEDIA_PLAYPAUSE, 850, 30) ==
           CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_multitap_handle_button(
               &state, BUTTON_MULTIMEDIA_PLAYPAUSE | BUTTON_REL,
               BUTTON_MULTIMEDIA_PLAYPAUSE, 855, 30) ==
           CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_multitap_handle_button(
               &state, BUTTON_MULTIMEDIA_PLAYPAUSE,
               BUTTON_MULTIMEDIA_PLAYPAUSE, 865, 30) ==
           CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_multitap_handle_button(
               &state, BUTTON_MULTIMEDIA_PLAYPAUSE | BUTTON_REL,
               BUTTON_MULTIMEDIA_PLAYPAUSE, 870, 30) ==
           CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_remote_multitap_tick(&state, 900) ==
           CRAZYPOD_REMOTE_MULTITAP_NEXT);

    crazypod_remote_multitap_reset(&state);
    assert(crazypod_multitap_handle_button(
               &state, BUTTON_MULTIMEDIA_PLAYPAUSE,
               BUTTON_MULTIMEDIA_PLAYPAUSE, 910, 30) ==
           CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_multitap_handle_button(
               &state, BUTTON_MULTIMEDIA_PLAYPAUSE | BUTTON_REL,
               BUTTON_MULTIMEDIA_PLAYPAUSE, 915, 30) ==
           CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_multitap_handle_button(
               &state, BUTTON_MULTIMEDIA_PLAYPAUSE,
               BUTTON_MULTIMEDIA_PLAYPAUSE, 925, 30) ==
           CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_multitap_handle_button(
               &state, BUTTON_MULTIMEDIA_PLAYPAUSE | BUTTON_REL,
               BUTTON_MULTIMEDIA_PLAYPAUSE, 930, 30) ==
           CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_multitap_handle_button(
               &state, BUTTON_MULTIMEDIA_PLAYPAUSE,
               BUTTON_MULTIMEDIA_PLAYPAUSE, 940, 30) ==
           CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_multitap_handle_button(
               &state, BUTTON_MULTIMEDIA_PLAYPAUSE | BUTTON_REL,
               BUTTON_MULTIMEDIA_PLAYPAUSE, 945, 30) ==
           CRAZYPOD_REMOTE_MULTITAP_PREVIOUS);

    /* Universal Dock lock-screen captures include an 840 ms release-to-press
     * gap. A 900 ms lock-screen window must retain that first tap. */
    crazypod_remote_multitap_reset(&state);
    assert(crazypod_remote_multitap_handle_down(
               &state, BUTTON_RC_VOL_DOWN, 900, 90) ==
           CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_remote_multitap_handle_down(
               &state, BUTTON_RC_VOL_DOWN | BUTTON_REL,
               918, 90) == CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_remote_multitap_handle_down(
               &state, BUTTON_RC_VOL_DOWN, 1002, 90) ==
           CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_remote_multitap_handle_down(
               &state, BUTTON_RC_VOL_DOWN | BUTTON_REL,
               1020, 90) == CRAZYPOD_REMOTE_MULTITAP_NONE);
    assert(crazypod_remote_multitap_tick(&state, 1110) ==
           CRAZYPOD_REMOTE_MULTITAP_NEXT);

    return 0;
}
