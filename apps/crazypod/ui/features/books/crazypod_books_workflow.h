#ifndef CRAZYPOD_BOOKS_WORKFLOW_H
#define CRAZYPOD_BOOKS_WORKFLOW_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

struct crazypod_books_workflow_host {
    lv_obj_t *parent;
    const lv_font_t *metadata_font;
    const uint32_t *page_colors;
    const uint32_t *ink_colors;
    void (*set_status_palette)(uint32_t foreground, uint32_t background);
    void (*status_foreground)(void);
    void (*present)(void);
    void (*render_route)(bool transition);
    void (*push_reader)(int book_index);
};

void crazypod_books_workflow_configure(
    const struct crazypod_books_workflow_host *host);
void crazypod_books_workflow_reset_view(void);
void crazypod_books_workflow_invalidate_metadata(void);
void crazypod_books_workflow_ensure_metadata(void);
void crazypod_books_workflow_apply_font_size(int value);
void crazypod_books_workflow_begin_reader(
    int index, uint32_t offset);

#endif
