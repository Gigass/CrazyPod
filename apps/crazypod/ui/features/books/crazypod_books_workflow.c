#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "../../../crazypod_book_cover.h"
#include "../../../crazypod_books.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_book_session.h"
#include "crazypod_books_workflow.h"

static struct crazypod_books_workflow_host workflow_host;
static bool metadata_ready;
static lv_obj_t *progress_fill;
static lv_obj_t *progress_label;
static lv_obj_t *percent_label;

void crazypod_books_workflow_configure(
    const struct crazypod_books_workflow_host *host)
{
    memset(&workflow_host, 0, sizeof(workflow_host));
    if(host != NULL)
        workflow_host = *host;
}

void crazypod_books_workflow_reset_view(void)
{
    progress_fill = NULL;
    progress_label = NULL;
    percent_label = NULL;
}

void crazypod_books_workflow_invalidate_metadata(void)
{
    metadata_ready = false;
    crazypod_book_session_reset();
}

static void render_loading(
    const struct crazypod_book *book,
    const char *title, const char *detail)
{
    int theme = crazypod_books_theme();
    uint32_t page_color = workflow_host.page_colors[theme];
    uint32_t ink_color = workflow_host.ink_colors[theme];
    lv_obj_t *label;
    lv_obj_t *track;

    lv_obj_clean(workflow_host.parent);
    lv_obj_set_style_bg_color(
        workflow_host.parent, lv_color_hex(page_color), 0);
    lv_obj_set_style_bg_opa(
        workflow_host.parent, LV_OPA_COVER, 0);
    crazypod_ui_widget_box(
        workflow_host.parent, 0, 0, LCD_WIDTH, LCD_HEIGHT,
        0, page_color, LV_OPA_COVER);
    workflow_host.set_status_palette(ink_color, page_color);

    label = crazypod_ui_widget_label(
        workflow_host.parent,
        title != NULL ? title : "Preparing Book",
        &lv_font_montserrat_16, ink_color, LV_OPA_COVER);
    lv_obj_set_width(label, 280);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 20, 57);

    label = crazypod_ui_widget_label(
        workflow_host.parent,
        book != NULL && book->title[0] != '\0'
            ? book->title : "Reading local book data",
        workflow_host.metadata_font, ink_color, 180);
    lv_obj_set_width(label, 260);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 30, 88);

    track = crazypod_ui_widget_box(
        workflow_host.parent, 40, 126, 240, 7,
        LV_RADIUS_CIRCLE, ink_color, 32);
    progress_fill = crazypod_ui_widget_box(
        track, 0, 0, 2, 7,
        LV_RADIUS_CIRCLE, ink_color, 220);
    progress_label = crazypod_ui_widget_label(
        workflow_host.parent, "Starting",
        &lv_font_montserrat_10, ink_color, 190);
    lv_obj_set_width(progress_label, 220);
    lv_label_set_long_mode(
        progress_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(progress_label, 40, 145);
    percent_label = crazypod_ui_widget_label(
        workflow_host.parent, "0%",
        &lv_font_montserrat_10, ink_color, 190);
    lv_obj_set_width(percent_label, 42);
    lv_obj_set_style_text_align(
        percent_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(percent_label, 238, 145);
    label = crazypod_ui_widget_label(
        workflow_host.parent, detail != NULL ? detail : "",
        &lv_font_montserrat_8, ink_color, 105);
    lv_obj_set_width(label, 280);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 20, 181);
    workflow_host.status_foreground();
    lv_refr_now(NULL);
    workflow_host.present();
}

static void update_progress(
    int percent, const char *stage, void *context)
{
    char value[16];
    int width;

    (void)context;
    if(progress_fill == NULL ||
       progress_label == NULL || percent_label == NULL)
        return;
    if(percent < 0)
        percent = 0;
    if(percent > 100)
        percent = 100;
    width = percent * 240 / 100;
    if(width < 2)
        width = 2;
    lv_obj_set_width(progress_fill, width);
    lv_label_set_text(
        progress_label, stage != NULL ? stage : "Preparing");
    snprintf(value, sizeof(value), "%d%%", percent);
    lv_label_set_text(percent_label, value);
    lv_refr_now(NULL);
    workflow_host.present();
}

static void load_metadata(int start_percent, int end_percent)
{
    int count = crazypod_books_count();
    int span = end_percent - start_percent;
    int i;

    if(span < 0)
        span = 0;
    if(count <= 0) {
        update_progress(end_percent, "Library is empty", NULL);
        return;
    }
    for(i = 0; i < count; ++i) {
        update_progress(
            start_percent + span * i / count,
            "Reading book titles and covers", NULL);
        crazypod_book_probe(i);
    }
    update_progress(end_percent, "Finalizing library", NULL);
}

void crazypod_books_workflow_ensure_metadata(void)
{
    bool scan_needed = crazypod_books_scan_needed();

    if(metadata_ready && !scan_needed)
        return;
    render_loading(
        NULL, "Loading Library",
        "Reading final book titles and covers");
    if(scan_needed) {
        update_progress(6, "Scanning Books folder", NULL);
        crazypod_books_scan();
    }
    load_metadata(16, 96);
    metadata_ready = true;
    update_progress(100, "Library ready", NULL);
}

void crazypod_books_workflow_apply_font_size(int value)
{
    const struct crazypod_book *book =
        crazypod_book_get(crazypod_book_session_index());
    bool needs_reflow =
        book != NULL && crazypod_book_session_has_text();

    if(value == crazypod_books_font_size()) {
        workflow_host.render_route(false);
        return;
    }
    if(needs_reflow) {
        render_loading(
            book, "Reflowing Text",
            "Keeping your current reading position");
        update_progress(20, "Applying text size", NULL);
    }
    crazypod_books_set_font_size(value);
    if(needs_reflow) {
        bool loaded;
        int index = crazypod_book_session_index();
        uint32_t offset = crazypod_book_session_offset();

        update_progress(62, "Rebuilding current page", NULL);
        crazypod_book_session_begin(index);
        loaded = crazypod_book_session_load(index, offset);
        update_progress(
            100, loaded ? "Text ready"
                        : "Could not reload page", NULL);
    }
    workflow_host.render_route(false);
}

void crazypod_books_workflow_rescan(void)
{
    render_loading(
        NULL, "Scanning Books",
        "Refreshing imported books and cover data");
    metadata_ready = false;
    update_progress(12, "Resetting cover cache", NULL);
    crazypod_book_cover_reset();
    update_progress(20, "Reading Books folders", NULL);
    crazypod_books_scan();
    load_metadata(30, 94);
    metadata_ready = true;
    crazypod_book_session_reset();
    update_progress(100, "Library ready", NULL);
    workflow_host.render_route(false);
}

void crazypod_books_workflow_begin_reader(
    int index, uint32_t offset)
{
    const struct crazypod_book *book;
    bool show_progress;
    bool ready;
    bool loaded = false;

    book = crazypod_book_get(index);
    if(book == NULL)
        return;
    show_progress =
        book->format == CRAZYPOD_BOOK_EPUB &&
        book->content_size == 0;
    if(show_progress)
        render_loading(
            book, "Preparing Book",
            "First open creates a local reading cache");
    ready = show_progress
        ? crazypod_book_prepare_with_progress(
              index, update_progress, NULL)
        : crazypod_book_prepare(index);
    book = crazypod_book_get(index);
    if(book == NULL)
        return;
    if(book->content_size > 0 && offset >= book->content_size)
        offset = 0;
    crazypod_book_session_begin(index);
    if(show_progress)
        update_progress(
            94, ready ? "Loading first page"
                      : "Preparing error details", NULL);
    if(ready)
        loaded = crazypod_book_session_load(index, offset);
    if(!loaded) {
        crazypod_book_session_set_error(
            book->format == CRAZYPOD_BOOK_EPUB
                ? "This EPUB is damaged, encrypted, or unsupported."
                : "Unable to open this book.");
    }
    else {
        if(show_progress)
            update_progress(98, "Saving reading position", NULL);
        crazypod_book_set_progress(index, offset);
    }
    if(show_progress)
        update_progress(
            100, loaded ? "Opening reader"
                        : "Showing book error", NULL);
    workflow_host.push_reader(index);
}

#endif
