#include "config.h"

#ifdef IPOD_6G

#include "../../../crazypod_notes.h"
#include "../../presentation/crazypod_ui_text.h"
#include "crazypod_notes_actions.h"
#include "crazypod_notes_controller.h"
#include "crazypod_notes_feature.h"
#include "crazypod_notes_screen.h"

#define EDITOR_ACTION_COUNT 3
#define EDITOR_CHARACTER_COUNT 36

static const char *const editor_characters[EDITOR_CHARACTER_COUNT] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
    "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
    "U", "V", "W", "X", "Y", "Z",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"
};

static int home_note_start(void)
{
    return crazypod_notes_controller_draft_available() ? 2 : 1;
}

static int home_deleted_index(void)
{
    return home_note_start() + crazypod_notes_count(false) + 1;
}

static const char *editor_title(int index, const char *final_action)
{
    if(index >= 0 && index < EDITOR_CHARACTER_COUNT)
        return editor_characters[index];
    if(index == EDITOR_CHARACTER_COUNT)
        return "Space";
    if(index == EDITOR_CHARACTER_COUNT + 1)
        return "Backspace";
    if(index == EDITOR_CHARACTER_COUNT + 2)
        return final_action;
    return "";
}

int crazypod_notes_feature_item_count(
    const struct route_state *state)
{
    switch(state->route) {
    case NOTES_ROUTE_MENU:
        return (crazypod_notes_controller_draft_available() ? 2 : 1) +
            crazypod_notes_count(false) + 2;
    case NOTES_ROUTE_COMPOSER:
    case NOTES_ROUTE_SEARCH:
        return EDITOR_CHARACTER_COUNT + EDITOR_ACTION_COUNT;
    case NOTES_ROUTE_EXIT_ACTIONS:
        return 3;
    case NOTES_ROUTE_DISCARD_CONFIRM:
    case NOTES_ROUTE_DELETE_CONFIRM:
    case NOTES_ROUTE_PERMANENT_CONFIRM:
    case NOTES_ROUTE_EMPTY_TRASH_CONFIRM:
        return 1;
    case NOTES_ROUTE_SEARCH_RESULTS:
        return crazypod_notes_search_count(
            crazypod_notes_controller_query());
    case NOTES_ROUTE_READER: {
        int lines = crazypod_ui_text_note_line_count(
            crazypod_notes_controller_reader_body());

        return lines > 9 ? lines - 8 : 1;
    }
    case NOTES_ROUTE_ACTIONS:
        return 4;
    case NOTES_ROUTE_DELETED:
        return crazypod_notes_count(true) > 0
            ? crazypod_notes_count(true) + 1 : 0;
    case NOTES_ROUTE_DELETED_ACTIONS:
        return 2;
    default:
        return 0;
    }
}

const char *crazypod_notes_feature_title(
    const struct route_state *state)
{
    switch(state->route) {
    case NOTES_ROUTE_MENU:
        return "NOTES";
    case NOTES_ROUTE_COMPOSER:
        return crazypod_notes_controller_body_active()
            ? "EDIT BODY" : "EDIT TITLE";
    case NOTES_ROUTE_EXIT_ACTIONS:
        return "UNSAVED NOTE";
    case NOTES_ROUTE_DISCARD_CONFIRM:
        return "DISCARD DRAFT";
    case NOTES_ROUTE_SEARCH:
        return "SEARCH NOTES";
    case NOTES_ROUTE_SEARCH_RESULTS:
        return "RESULTS";
    case NOTES_ROUTE_READER: {
        const struct crazypod_note *note =
            crazypod_note_find((uint32_t)state->group);

        return note != NULL ? note->title : "NOTE";
    }
    case NOTES_ROUTE_ACTIONS:
        return "NOTE ACTIONS";
    case NOTES_ROUTE_DELETED:
        return "DELETED";
    case NOTES_ROUTE_DELETED_ACTIONS:
        return "DELETED NOTE";
    case NOTES_ROUTE_DELETE_CONFIRM:
        return "DELETE NOTE";
    case NOTES_ROUTE_PERMANENT_CONFIRM:
        return "ERASE NOTE";
    case NOTES_ROUTE_EMPTY_TRASH_CONFIRM:
        return "EMPTY DELETED";
    default:
        return "";
    }
}

bool crazypod_notes_feature_item_title(
    const struct route_state *state, int index,
    const char **title)
{
    switch(state->route) {
    case NOTES_ROUTE_MENU: {
        const struct crazypod_note *note;
        int deleted_index = home_deleted_index();
        int note_index;

        if(index == 0)
            *title = "New Note";
        else if(crazypod_notes_controller_draft_available() &&
                index == 1)
            *title = "Continue Draft";
        else if(index == deleted_index - 1)
            *title = "Search";
        else if(index == deleted_index)
            *title = "Deleted";
        else {
            note_index = index - home_note_start();
            note = note_index >= 0
                ? crazypod_note_get(false, note_index) : NULL;
            *title = note != NULL ? note->title : "";
        }
        return true;
    }
    case NOTES_ROUTE_COMPOSER:
        *title = editor_title(index, "Save Note");
        return true;
    case NOTES_ROUTE_EXIT_ACTIONS:
        *title = index == 0 ? "Save" :
            index == 1 ? "Keep" :
            index == 2 ? "Discard" : "";
        return true;
    case NOTES_ROUTE_DISCARD_CONFIRM:
        *title = "Hold Center to Discard";
        return true;
    case NOTES_ROUTE_SEARCH:
        *title = editor_title(index, "View Results");
        return true;
    case NOTES_ROUTE_SEARCH_RESULTS: {
        const struct crazypod_note *note =
            crazypod_notes_search_get(
                crazypod_notes_controller_query(), index);

        *title = note != NULL ? note->title : "";
        return true;
    }
    case NOTES_ROUTE_ACTIONS: {
        const struct crazypod_note *note =
            crazypod_note_find((uint32_t)state->group);

        if(index == 0)
            *title = note != NULL && note->pinned ? "Unpin" : "Pin";
        else
            *title = index == 1 ? "Edit" :
                index == 2 ? "Duplicate" :
                index == 3 ? "Delete" : "";
        return true;
    }
    case NOTES_ROUTE_DELETED:
        if(index == crazypod_notes_count(true))
            *title = "Empty Deleted";
        else {
            const struct crazypod_note *note =
                crazypod_note_get(true, index);

            *title = note != NULL ? note->title : "";
        }
        return true;
    case NOTES_ROUTE_DELETED_ACTIONS:
        *title = index == 0 ? "Restore" :
            index == 1 ? "Erase Forever" : "";
        return true;
    case NOTES_ROUTE_DELETE_CONFIRM:
        *title = "Hold Center to Delete";
        return true;
    case NOTES_ROUTE_PERMANENT_CONFIRM:
        *title = "Hold Center to Erase";
        return true;
    case NOTES_ROUTE_EMPTY_TRASH_CONFIRM:
        *title = "Hold Center to Empty";
        return true;
    case NOTES_ROUTE_READER:
        *title = "Reader";
        return true;
    default:
        return false;
    }
}

bool crazypod_notes_feature_activate(
    const struct route_state *state,
    const struct crazypod_notes_activation_host *host)
{
    const struct crazypod_notes_action action =
        crazypod_notes_actions_activate(state);

    if(action.kind == CRAZYPOD_NOTES_ACTION_UNHANDLED)
        return false;
    switch(action.kind) {
    case CRAZYPOD_NOTES_ACTION_RENDER:
        host->render(false);
        break;
    case CRAZYPOD_NOTES_ACTION_PUSH:
        host->push(action.route, action.group);
        break;
    case CRAZYPOD_NOTES_ACTION_POP:
        host->pop();
        break;
    case CRAZYPOD_NOTES_ACTION_POP_COMPOSER:
        host->pop_composer();
        break;
    case CRAZYPOD_NOTES_ACTION_OPEN_COMPOSER:
        host->open_composer(
            action.note_id, action.resume_draft);
        break;
    case CRAZYPOD_NOTES_ACTION_OPEN_READER:
        host->open_reader(action.note_id);
        break;
    case CRAZYPOD_NOTES_ACTION_RESET_OPEN_READER:
        host->reset_open_reader(action.note_id);
        break;
    case CRAZYPOD_NOTES_ACTION_COMMIT_EDITOR:
        host->commit_editor();
        break;
    case CRAZYPOD_NOTES_ACTION_NONE:
    case CRAZYPOD_NOTES_ACTION_UNHANDLED:
    default:
        break;
    }
    return true;
}

bool crazypod_notes_feature_render(
    const struct route_state *state, lv_obj_t *parent)
{
    if(state->route == NOTES_ROUTE_COMPOSER) {
        const struct crazypod_notes_screen_model model = {
            .editor = crazypod_notes_controller_editor(),
            .dirty = crazypod_notes_controller_dirty(),
            .body_active =
                crazypod_notes_controller_body_active(),
            .title_cursor =
                crazypod_notes_controller_title_cursor(),
            .body_cursor =
                crazypod_notes_controller_body_cursor(),
        };
        const char *selected_title = "";

        (void)crazypod_notes_feature_item_title(
            state, state->selected, &selected_title);
        crazypod_notes_screen_render_composer(
            parent, &model, selected_title);
        return true;
    }
    if(state->route != NOTES_ROUTE_READER)
        return false;
    crazypod_notes_screen_render_reader(
        parent, (uint32_t)state->group, state->selected,
        crazypod_notes_controller_reader_body());
    return true;
}

#endif
