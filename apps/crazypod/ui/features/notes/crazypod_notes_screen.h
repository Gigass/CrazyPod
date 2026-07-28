#ifndef CRAZYPOD_NOTES_SCREEN_H
#define CRAZYPOD_NOTES_SCREEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"

#include "../../../crazypod_notes.h"

struct crazypod_notes_screen_model {
    const struct crazypod_note_draft *editor;
    bool dirty;
    bool body_active;
    size_t title_cursor;
    size_t body_cursor;
};

void crazypod_notes_screen_render_composer(
    lv_obj_t *content, const struct crazypod_notes_screen_model *model,
    const char *selection);
void crazypod_notes_screen_render_reader(
    lv_obj_t *content, uint32_t note_id, int first_line,
    const char *body);

#endif
