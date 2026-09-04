#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "kernel.h"

#include "../../../crazypod_notes.h"
#include "../../presentation/crazypod_ui_text.h"
#include "crazypod_notes_controller.h"

#define CRAZYPOD_NOTES_QUERY_SIZE 33

static struct crazypod_note_draft editor;
static struct crazypod_note_draft baseline;
static bool draft_available;
static bool body_active;
static size_t title_cursor;
static size_t body_cursor;
static bool draft_save_pending;
static long draft_save_due;
static char reader_body[CRAZYPOD_NOTE_BODY_SIZE];
static char query[CRAZYPOD_NOTES_QUERY_SIZE];

static char *active_text(size_t **cursor, size_t *capacity)
{
    if(body_active) {
        if(cursor != NULL)
            *cursor = &body_cursor;
        if(capacity != NULL)
            *capacity = sizeof(editor.body);
        return editor.body;
    }

    if(cursor != NULL)
        *cursor = &title_cursor;
    if(capacity != NULL)
        *capacity = sizeof(editor.title);
    return editor.title;
}

void crazypod_notes_controller_refresh_draft(void)
{
    struct crazypod_note_draft loaded;

    if(crazypod_notes_controller_dirty())
        crazypod_notes_controller_save_draft();
    draft_available = crazypod_note_draft_load(&loaded);
    if(draft_available)
        editor = loaded;
    else
        memset(&editor, 0, sizeof(editor));
    baseline = editor;
    title_cursor = strlen(editor.title);
    body_cursor = strlen(editor.body);
    body_active = false;
    draft_save_pending = false;
    reader_body[0] = '\0';
    query[0] = '\0';
}

bool crazypod_notes_controller_draft_available(void)
{
    return draft_available;
}

void crazypod_notes_controller_begin(uint32_t id, bool resume_draft)
{
    const struct crazypod_note *note;

    memset(&editor, 0, sizeof(editor));
    if(resume_draft) {
        if(!crazypod_note_draft_load(&editor))
            memset(&editor, 0, sizeof(editor));
    }
    else if(id != 0) {
        note = crazypod_note_find(id);
        if(note != NULL) {
            editor.source_id = id;
            snprintf(editor.title, sizeof(editor.title), "%s", note->title);
            crazypod_note_read_body(
                id, editor.body, sizeof(editor.body));
        }
    }

    baseline = editor;
    title_cursor = strlen(editor.title);
    body_cursor = strlen(editor.body);
    body_active = false;
    draft_save_pending = false;
}

const struct crazypod_note_draft *crazypod_notes_controller_editor(void)
{
    return &editor;
}

bool crazypod_notes_controller_dirty(void)
{
    return editor.source_id != baseline.source_id ||
           strcmp(editor.title, baseline.title) != 0 ||
           strcmp(editor.body, baseline.body) != 0;
}

bool crazypod_notes_controller_body_active(void)
{
    return body_active;
}

void crazypod_notes_controller_toggle_field(void)
{
    body_active = !body_active;
}

size_t crazypod_notes_controller_title_cursor(void)
{
    return title_cursor;
}

size_t crazypod_notes_controller_body_cursor(void)
{
    return body_cursor;
}

void crazypod_notes_controller_insert(const char *text)
{
    size_t capacity;
    size_t *cursor;
    char *target = active_text(&cursor, &capacity);

    crazypod_ui_text_insert(target, capacity, cursor, text);
}

void crazypod_notes_controller_backspace(void)
{
    size_t *cursor;
    char *target = active_text(&cursor, NULL);

    crazypod_ui_text_backspace_at(target, cursor);
}

void crazypod_notes_controller_move_cursor(int direction)
{
    size_t *cursor;
    char *target = active_text(&cursor, NULL);

    crazypod_ui_text_move_cursor(target, cursor, direction);
}

void crazypod_notes_controller_save_draft(void)
{
    if(editor.title[0] == '\0' && editor.body[0] == '\0') {
        crazypod_note_draft_clear();
        draft_available = false;
        draft_save_pending = false;
        return;
    }

    draft_available = crazypod_note_draft_save(&editor);
    draft_save_pending = false;
}

void crazypod_notes_controller_schedule_draft(void)
{
    draft_save_pending = true;
    draft_save_due = current_tick + HZ * 2;
}

void crazypod_notes_controller_service(void)
{
    if(draft_save_pending &&
       !TIME_BEFORE(current_tick, draft_save_due))
        crazypod_notes_controller_save_draft();
}

void crazypod_notes_controller_discard(void)
{
    crazypod_note_draft_clear();
    draft_available = false;
    draft_save_pending = false;
    memset(&editor, 0, sizeof(editor));
    memset(&baseline, 0, sizeof(baseline));
    title_cursor = 0;
    body_cursor = 0;
    body_active = false;
}

uint32_t crazypod_notes_controller_commit(void)
{
    uint32_t id = crazypod_note_save(
        editor.source_id, editor.title, editor.body);

    if(id == 0) {
        crazypod_notes_controller_save_draft();
        return 0;
    }

    crazypod_note_draft_clear();
    draft_available = false;
    draft_save_pending = false;
    memset(&editor, 0, sizeof(editor));
    memset(&baseline, 0, sizeof(baseline));
    title_cursor = 0;
    body_cursor = 0;
    body_active = false;
    return id;
}

void crazypod_notes_controller_load_reader(uint32_t id)
{
    reader_body[0] = '\0';
    crazypod_note_read_body(id, reader_body, sizeof(reader_body));
}

const char *crazypod_notes_controller_reader_body(void)
{
    return reader_body;
}

const char *crazypod_notes_controller_query(void)
{
    return query;
}

void crazypod_notes_controller_clear_query(void)
{
    query[0] = '\0';
}

void crazypod_notes_controller_append_query(const char *text)
{
    crazypod_ui_text_append(query, sizeof(query), text);
}

void crazypod_notes_controller_backspace_query(void)
{
    crazypod_ui_text_backspace(query);
}

#endif
