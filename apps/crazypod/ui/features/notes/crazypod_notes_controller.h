#ifndef CRAZYPOD_NOTES_CONTROLLER_H
#define CRAZYPOD_NOTES_CONTROLLER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../../crazypod_notes.h"

void crazypod_notes_controller_refresh_draft(void);
bool crazypod_notes_controller_draft_available(void);

void crazypod_notes_controller_begin(uint32_t id, bool resume_draft);
const struct crazypod_note_draft *crazypod_notes_controller_editor(void);
bool crazypod_notes_controller_dirty(void);
bool crazypod_notes_controller_body_active(void);
void crazypod_notes_controller_toggle_field(void);
size_t crazypod_notes_controller_title_cursor(void);
size_t crazypod_notes_controller_body_cursor(void);
void crazypod_notes_controller_insert(const char *text);
void crazypod_notes_controller_backspace(void);
void crazypod_notes_controller_move_cursor(int direction);

void crazypod_notes_controller_save_draft(void);
void crazypod_notes_controller_schedule_draft(void);
void crazypod_notes_controller_service(void);
void crazypod_notes_controller_discard(void);
uint32_t crazypod_notes_controller_commit(void);

void crazypod_notes_controller_load_reader(uint32_t id);
const char *crazypod_notes_controller_reader_body(void);

const char *crazypod_notes_controller_query(void);
void crazypod_notes_controller_clear_query(void);
void crazypod_notes_controller_append_query(const char *text);
void crazypod_notes_controller_backspace_query(void);

#endif
