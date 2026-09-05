#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include "../../../crazypod_notes.h"
#include "../../presentation/crazypod_ui_text.h"
#include "crazypod_notes_actions.h"
#include "crazypod_notes_controller.h"
#include "crazypod_notes_confirmation.h"
#include "crazypod_notes_feature.h"
#include "crazypod_notes_input.h"
#include "crazypod_notes_preview.h"
#include "crazypod_notes_screen.h"

#define EDITOR_ACTION_COUNT 3
#define EDITOR_CHARACTER_COUNT 36

static const char *const editor_characters[EDITOR_CHARACTER_COUNT] = {
    CP_TR("A"), CP_TR("B"), CP_TR("C"), CP_TR("D"), CP_TR("E"), CP_TR("F"), CP_TR("G"), CP_TR("H"), CP_TR("I"), CP_TR("J"),
    CP_TR("K"), CP_TR("L"), CP_TR("M"), CP_TR("N"), CP_TR("O"), CP_TR("P"), CP_TR("Q"), CP_TR("R"), CP_TR("S"), CP_TR("T"),
    CP_TR("U"), CP_TR("V"), CP_TR("W"), "X", CP_TR("Y"), CP_TR("Z"),
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
        return CP_TR("Space");
    if(index == EDITOR_CHARACTER_COUNT + 1)
        return CP_TR("Backspace");
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
        return CP_TR("NOTES");
    case NOTES_ROUTE_COMPOSER:
        return crazypod_notes_controller_body_active()
            ? CP_TR("EDIT BODY") : CP_TR("EDIT TITLE");
    case NOTES_ROUTE_EXIT_ACTIONS:
        return CP_TR("UNSAVED NOTE");
    case NOTES_ROUTE_DISCARD_CONFIRM:
        return CP_TR("DISCARD DRAFT");
    case NOTES_ROUTE_SEARCH:
        return CP_TR("SEARCH NOTES");
    case NOTES_ROUTE_SEARCH_RESULTS:
        return CP_TR("RESULTS");
    case NOTES_ROUTE_READER: {
        const struct crazypod_note *note =
            crazypod_note_find((uint32_t)state->group);

        return note != NULL ? note->title : CP_TR("NOTE");
    }
    case NOTES_ROUTE_ACTIONS:
        return CP_TR("NOTE ACTIONS");
    case NOTES_ROUTE_DELETED:
        return CP_TR("DELETED");
    case NOTES_ROUTE_DELETED_ACTIONS:
        return CP_TR("DELETED NOTE");
    case NOTES_ROUTE_DELETE_CONFIRM:
        return CP_TR("DELETE NOTE");
    case NOTES_ROUTE_PERMANENT_CONFIRM:
        return CP_TR("ERASE NOTE");
    case NOTES_ROUTE_EMPTY_TRASH_CONFIRM:
        return CP_TR("EMPTY DELETED");
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
            *title = CP_TR("New Note");
        else if(crazypod_notes_controller_draft_available() &&
                index == 1)
            *title = CP_TR("Continue Draft");
        else if(index == deleted_index - 1)
            *title = CP_TR("Search");
        else if(index == deleted_index)
            *title = CP_TR("Deleted");
        else {
            note_index = index - home_note_start();
            note = note_index >= 0
                ? crazypod_note_get(false, note_index) : NULL;
            *title = note != NULL ? note->title : "";
        }
        return true;
    }
    case NOTES_ROUTE_COMPOSER:
        *title = editor_title(index, CP_TR("Save Note"));
        return true;
    case NOTES_ROUTE_EXIT_ACTIONS:
        *title = index == 0 ? CP_TR("Save") :
            index == 1 ? CP_TR("Keep") :
            index == 2 ? CP_TR("Discard") : "";
        return true;
    case NOTES_ROUTE_DISCARD_CONFIRM:
        *title = CP_TR("Hold Center to Discard");
        return true;
    case NOTES_ROUTE_SEARCH:
        *title = editor_title(index, CP_TR("View Results"));
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
            *title = note != NULL && note->pinned ? CP_TR("Unpin") : CP_TR("Pin");
        else
            *title = index == 1 ? CP_TR("Edit") :
                index == 2 ? CP_TR("Duplicate") :
                index == 3 ? CP_TR("Delete") : "";
        return true;
    }
    case NOTES_ROUTE_DELETED:
        if(index == crazypod_notes_count(true))
            *title = CP_TR("Empty Deleted");
        else {
            const struct crazypod_note *note =
                crazypod_note_get(true, index);

            *title = note != NULL ? note->title : "";
        }
        return true;
    case NOTES_ROUTE_DELETED_ACTIONS:
        *title = index == 0 ? CP_TR("Restore") :
            index == 1 ? CP_TR("Erase Forever") : "";
        return true;
    case NOTES_ROUTE_DELETE_CONFIRM:
        *title = CP_TR("Hold Center to Delete");
        return true;
    case NOTES_ROUTE_PERMANENT_CONFIRM:
        *title = CP_TR("Hold Center to Erase");
        return true;
    case NOTES_ROUTE_EMPTY_TRASH_CONFIRM:
        *title = CP_TR("Hold Center to Empty");
        return true;
    case NOTES_ROUTE_READER:
        *title = CP_TR("Reader");
        return true;
    default:
        return false;
    }
}

enum crazypod_menu_icon crazypod_notes_feature_item_icon(
    const struct route_state *state, int index)
{
    const struct crazypod_note *note;

    if(index < 0)
        return CRAZYPOD_MENU_ICON_NONE;
    switch(state->route) {
    case NOTES_ROUTE_MENU: {
        int deleted_index = home_deleted_index();
        int note_index;

        if(index == 0)
            return CRAZYPOD_MENU_ICON_ADD_NOTE;
        if(crazypod_notes_controller_draft_available() && index == 1)
            return CRAZYPOD_MENU_ICON_DRAFT;
        if(index == deleted_index - 1)
            return CRAZYPOD_MENU_ICON_SEARCH;
        if(index == deleted_index)
            return CRAZYPOD_MENU_ICON_TRASH;
        note_index = index - home_note_start();
        note = note_index >= 0
            ? crazypod_note_get(false, note_index) : NULL;
        return note != NULL && note->pinned
            ? CRAZYPOD_MENU_ICON_PIN : CRAZYPOD_MENU_ICON_NOTE;
    }
    case NOTES_ROUTE_EXIT_ACTIONS:
        return index == 0 ? CRAZYPOD_MENU_ICON_SAVE :
            index == 1 ? CRAZYPOD_MENU_ICON_DRAFT :
            index == 2 ? CRAZYPOD_MENU_ICON_TRASH :
            CRAZYPOD_MENU_ICON_NONE;
    case NOTES_ROUTE_SEARCH_RESULTS:
        note = crazypod_notes_search_get(
            crazypod_notes_controller_query(), index);
        return note != NULL && note->pinned
            ? CRAZYPOD_MENU_ICON_PIN : CRAZYPOD_MENU_ICON_NOTE;
    case NOTES_ROUTE_ACTIONS:
        return index == 0 ? CRAZYPOD_MENU_ICON_PIN :
            index == 1 ? CRAZYPOD_MENU_ICON_EDIT :
            index == 2 ? CRAZYPOD_MENU_ICON_DUPLICATE :
            index == 3 ? CRAZYPOD_MENU_ICON_TRASH :
            CRAZYPOD_MENU_ICON_NONE;
    case NOTES_ROUTE_DELETED:
        return index == crazypod_notes_count(true)
            ? CRAZYPOD_MENU_ICON_ERASE : CRAZYPOD_MENU_ICON_NOTE;
    case NOTES_ROUTE_DELETED_ACTIONS:
        return index == 0 ? CRAZYPOD_MENU_ICON_RESTORE :
            index == 1 ? CRAZYPOD_MENU_ICON_ERASE :
            CRAZYPOD_MENU_ICON_NONE;
    case NOTES_ROUTE_DISCARD_CONFIRM:
    case NOTES_ROUTE_DELETE_CONFIRM:
        return CRAZYPOD_MENU_ICON_TRASH;
    case NOTES_ROUTE_PERMANENT_CONFIRM:
    case NOTES_ROUTE_EMPTY_TRASH_CONFIRM:
        return CRAZYPOD_MENU_ICON_ERASE;
    case NOTES_ROUTE_READER:
        return CRAZYPOD_MENU_ICON_READING;
    case NOTES_ROUTE_COMPOSER:
    case NOTES_ROUTE_SEARCH:
    default:
        return CRAZYPOD_MENU_ICON_NONE;
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
    case CRAZYPOD_NOTES_ACTION_FAILED:
        host->operation_failed();
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

static struct crazypod_feature_input_context notes_input_context;

static void notes_input_render(void)
{
    notes_input_context.render(false);
}

static void show_exit_actions(void)
{
    notes_input_context.push(
        NOTES_ROUTE_EXIT_ACTIONS, -1);
}

static void show_search_results(void)
{
    notes_input_context.push(
        NOTES_ROUTE_SEARCH_RESULTS, -1);
}

bool crazypod_notes_feature_handle_input(
    const struct route_state *state,
    const struct crazypod_input_event *event,
    const struct crazypod_feature_input_context *context)
{
    const struct crazypod_notes_input_actions actions = {
        .move_selection = context->move,
        .activate = context->activate,
        .render = notes_input_render,
        .leave = context->pop,
        .show_exit_actions = show_exit_actions,
        .show_search_results = show_search_results,
    };

    notes_input_context = *context;
    return crazypod_notes_input_handle(
        state->route,
        crazypod_notes_controller_dirty(),
        event, &actions);
}

void crazypod_notes_feature_render_preview(
    lv_obj_t *parent, const struct route_state *state,
    const lv_font_t *metadata_font)
{
    crazypod_notes_preview_render(
        parent, state, metadata_font);
}

void crazypod_notes_feature_refresh_draft(void)
{
    crazypod_notes_controller_refresh_draft();
}

void crazypod_notes_feature_toggle_editor_field(void)
{
    crazypod_notes_controller_toggle_field();
}

void crazypod_notes_feature_begin_editor(
    uint32_t id, bool resume_draft)
{
    crazypod_notes_controller_begin(id, resume_draft);
}

void crazypod_notes_feature_load_reader(uint32_t id)
{
    crazypod_notes_controller_load_reader(id);
}

bool crazypod_notes_feature_editor_dirty(void)
{
    return crazypod_notes_controller_dirty();
}

void crazypod_notes_feature_service_editor(void)
{
    crazypod_notes_controller_service();
}

void crazypod_notes_feature_save_draft(void)
{
    if(crazypod_notes_controller_dirty())
        crazypod_notes_controller_save_draft();
}

uint32_t crazypod_notes_feature_commit_editor(void)
{
    return crazypod_notes_controller_commit();
}

bool crazypod_notes_feature_draft_available(void)
{
    return crazypod_notes_controller_draft_available();
}

struct crazypod_notes_confirmation_result
crazypod_notes_feature_confirm(
    const struct route_state *state, int route_depth)
{
    return crazypod_notes_confirmation_execute(
        state, route_depth);
}

#endif
