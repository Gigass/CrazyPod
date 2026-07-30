#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include "../../../crazypod_notes.h"
#include "crazypod_notes_controller.h"
#include "crazypod_notes_actions.h"

#define EDITOR_CHAR_COUNT 36

static const char *const editor_characters[EDITOR_CHAR_COUNT] = {
    CP_TR("A"), CP_TR("B"), CP_TR("C"), CP_TR("D"), CP_TR("E"), CP_TR("F"), CP_TR("G"), CP_TR("H"), CP_TR("I"), CP_TR("J"),
    CP_TR("K"), CP_TR("L"), CP_TR("M"), CP_TR("N"), CP_TR("O"), CP_TR("P"), CP_TR("Q"), CP_TR("R"), CP_TR("S"), CP_TR("T"),
    CP_TR("U"), CP_TR("V"), CP_TR("W"), "X", CP_TR("Y"), CP_TR("Z"),
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
};

static struct crazypod_notes_action action(
    enum crazypod_notes_action_kind kind)
{
    const struct crazypod_notes_action result = {
        .kind = kind,
    };
    return result;
}

static struct crazypod_notes_action push(
    enum crazypod_route route, int group)
{
    struct crazypod_notes_action result =
        action(CRAZYPOD_NOTES_ACTION_PUSH);

    result.route = route;
    result.group = group;
    return result;
}

static int note_start(void)
{
    return crazypod_notes_controller_draft_available() ? 2 : 1;
}

static int deleted_index(void)
{
    return note_start() + crazypod_notes_count(false) + 1;
}

static const struct crazypod_note *home_note(int index)
{
    int note_index = index - note_start();

    return note_index >= 0
        ? crazypod_note_get(false, note_index) : NULL;
}

struct crazypod_notes_action crazypod_notes_actions_activate(
    const struct route_state *state)
{
    struct crazypod_notes_action result;

    switch(state->route) {
    case NOTES_ROUTE_MENU: {
        const struct crazypod_note *note;

        if(state->selected == 0) {
            result = action(CRAZYPOD_NOTES_ACTION_OPEN_COMPOSER);
            return result;
        }
        if(crazypod_notes_controller_draft_available() &&
           state->selected == 1) {
            result = action(CRAZYPOD_NOTES_ACTION_OPEN_COMPOSER);
            result.resume_draft = true;
            return result;
        }
        if(state->selected == deleted_index() - 1) {
            crazypod_notes_controller_clear_query();
            return push(NOTES_ROUTE_SEARCH, -1);
        }
        if(state->selected == deleted_index())
            return push(NOTES_ROUTE_DELETED, -1);
        note = home_note(state->selected);
        result = action(CRAZYPOD_NOTES_ACTION_NONE);
        if(note != NULL) {
            result.kind = CRAZYPOD_NOTES_ACTION_OPEN_READER;
            result.note_id = note->id;
        }
        return result;
    }
    case NOTES_ROUTE_COMPOSER:
        if(state->selected < EDITOR_CHAR_COUNT)
            crazypod_notes_controller_insert(
                editor_characters[state->selected]);
        else if(state->selected == EDITOR_CHAR_COUNT)
            crazypod_notes_controller_insert(" ");
        else if(state->selected == EDITOR_CHAR_COUNT + 1)
            crazypod_notes_controller_backspace();
        else
            return action(CRAZYPOD_NOTES_ACTION_COMMIT_EDITOR);
        crazypod_notes_controller_schedule_draft();
        return action(CRAZYPOD_NOTES_ACTION_RENDER);
    case NOTES_ROUTE_EXIT_ACTIONS:
        if(state->selected == 0)
            return action(CRAZYPOD_NOTES_ACTION_COMMIT_EDITOR);
        if(state->selected == 1) {
            crazypod_notes_controller_save_draft();
            return action(CRAZYPOD_NOTES_ACTION_POP_COMPOSER);
        }
        return push(NOTES_ROUTE_DISCARD_CONFIRM, -1);
    case NOTES_ROUTE_DISCARD_CONFIRM:
    case NOTES_ROUTE_DELETE_CONFIRM:
    case NOTES_ROUTE_PERMANENT_CONFIRM:
    case NOTES_ROUTE_EMPTY_TRASH_CONFIRM:
        return action(CRAZYPOD_NOTES_ACTION_NONE);
    case NOTES_ROUTE_SEARCH:
        if(state->selected < EDITOR_CHAR_COUNT)
            crazypod_notes_controller_append_query(
                editor_characters[state->selected]);
        else if(state->selected == EDITOR_CHAR_COUNT)
            crazypod_notes_controller_append_query(" ");
        else if(state->selected == EDITOR_CHAR_COUNT + 1)
            crazypod_notes_controller_backspace_query();
        else if(crazypod_notes_controller_query()[0] != '\0')
            return push(NOTES_ROUTE_SEARCH_RESULTS, -1);
        return action(CRAZYPOD_NOTES_ACTION_RENDER);
    case NOTES_ROUTE_SEARCH_RESULTS: {
        const struct crazypod_note *note =
            crazypod_notes_search_get(
                crazypod_notes_controller_query(), state->selected);
        result = action(CRAZYPOD_NOTES_ACTION_NONE);
        if(note != NULL) {
            result.kind = CRAZYPOD_NOTES_ACTION_OPEN_READER;
            result.note_id = note->id;
        }
        return result;
    }
    case NOTES_ROUTE_READER:
        return push(NOTES_ROUTE_ACTIONS, state->group);
    case NOTES_ROUTE_ACTIONS: {
        uint32_t id = (uint32_t)state->group;
        const struct crazypod_note *note = crazypod_note_find(id);

        if(note == NULL)
            return action(CRAZYPOD_NOTES_ACTION_POP);
        if(state->selected == 0) {
            crazypod_note_set_pinned(id, !note->pinned);
            return action(CRAZYPOD_NOTES_ACTION_RENDER);
        }
        if(state->selected == 1) {
            result = action(CRAZYPOD_NOTES_ACTION_OPEN_COMPOSER);
            result.note_id = id;
            return result;
        }
        if(state->selected == 2) {
            uint32_t duplicate = crazypod_note_duplicate(id);

            result = action(CRAZYPOD_NOTES_ACTION_NONE);
            if(duplicate != 0) {
                result.kind =
                    CRAZYPOD_NOTES_ACTION_RESET_OPEN_READER;
                result.note_id = duplicate;
            }
            return result;
        }
        return push(NOTES_ROUTE_DELETE_CONFIRM, (int)id);
    }
    case NOTES_ROUTE_DELETED:
        if(state->selected == crazypod_notes_count(true))
            return push(NOTES_ROUTE_EMPTY_TRASH_CONFIRM, -1);
        else {
            const struct crazypod_note *note =
                crazypod_note_get(true, state->selected);

            return note != NULL
                ? push(NOTES_ROUTE_DELETED_ACTIONS, (int)note->id)
                : action(CRAZYPOD_NOTES_ACTION_NONE);
        }
    case NOTES_ROUTE_DELETED_ACTIONS:
        if(state->selected == 0) {
            crazypod_note_restore((uint32_t)state->group);
            return action(CRAZYPOD_NOTES_ACTION_POP);
        }
        return push(NOTES_ROUTE_PERMANENT_CONFIRM, state->group);
    default:
        return action(CRAZYPOD_NOTES_ACTION_UNHANDLED);
    }
}

#endif
