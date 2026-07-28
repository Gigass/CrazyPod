#include "config.h"

#ifdef IPOD_6G

#include "../../../crazypod_notes.h"
#include "crazypod_notes_controller.h"
#include "crazypod_notes_confirmation.h"

struct crazypod_notes_confirmation_result
crazypod_notes_confirmation_execute(
    const struct route_state *state, int route_depth)
{
    struct crazypod_notes_confirmation_result result = { 0 };

    if(state->route == NOTES_ROUTE_DELETE_CONFIRM) {
        (void)crazypod_note_move_to_trash((uint32_t)state->group);
        result.navigation = CRAZYPOD_NOTES_CONFIRMATION_RESET_MENU;
    }
    else if(state->route == NOTES_ROUTE_PERMANENT_CONFIRM) {
        (void)crazypod_note_delete_forever((uint32_t)state->group);
        result.navigation =
            CRAZYPOD_NOTES_CONFIRMATION_RESET_MENU_SHOW_DELETED;
    }
    else if(state->route == NOTES_ROUTE_EMPTY_TRASH_CONFIRM) {
        (void)crazypod_notes_empty_trash();
        result.navigation =
            CRAZYPOD_NOTES_CONFIRMATION_RESET_MENU_SHOW_DELETED;
    }
    else if(state->route == NOTES_ROUTE_DISCARD_CONFIRM) {
        crazypod_notes_controller_discard();
        result.navigation = CRAZYPOD_NOTES_CONFIRMATION_TRUNCATE;
        result.depth = route_depth - 3;
        if(result.depth < 1)
            result.depth = 1;
    }
    else
        return result;

    result.handled = true;
    return result;
}

#endif
