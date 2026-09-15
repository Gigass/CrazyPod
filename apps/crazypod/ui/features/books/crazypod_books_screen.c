#include "config.h"

#include "../../../crazypod_l10n.h"

#include <stdio.h>

#include "lvgl.h"

#include "../../../crazypod_audiobooks.h"
#include "../../../crazypod_books.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_books_screen.h"

#define CRAZYPOD_BOOKS_FONT (&lv_font_source_han_sans_sc_14_cjk)
#define CRAZYPOD_BOOKS_WHITE 0xFFFFFF
#define CRAZYPOD_BOOKS_PANEL 0x1B1B22

void crazypod_books_screen_render_reader(
    lv_obj_t *content, int book_index, uint32_t page_offset,
    const char *page_text, uint32_t page_color, uint32_t ink_color,
    bool toolbar_visible)
{
    const struct crazypod_book *book =
        crazypod_book_get(book_index);
    int theme = crazypod_books_theme();
    int font_size = crazypod_books_font_size();
    const lv_font_t *reader_font = font_size == 2
        ? &lv_font_source_han_sans_sc_16_cjk
        : CRAZYPOD_BOOKS_FONT;
    lv_obj_t *page;
    lv_obj_t *toolbar;
    lv_obj_t *label;
    char progress[24];
    uint32_t total = book != NULL && book->content_size > 0
        ? book->content_size : book != NULL ? book->size : 0;
    unsigned percent = total > 0
        ? page_offset * 100u / total : 0;

    page = crazypod_ui_widget_box(content, 0, 0, 320, 240, 0,
                    page_color, LV_OPA_COVER);
    label = crazypod_ui_widget_label(
        page,
        page_text[0] != '\0'
            ? page_text : CP_TR("This book could not be decoded."),
        reader_font, ink_color,
        LV_OPA_COVER);
    lv_obj_set_pos(label, 10, 36);
    lv_obj_set_width(label, 300);
    lv_obj_set_height(label, toolbar_visible ? 164 : 196);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    {
        static const int scales[] = { 224, 256, 256 };

        lv_obj_set_style_transform_scale_x(
            label, scales[font_size], 0);
        lv_obj_set_style_transform_scale_y(
            label, scales[font_size], 0);
        lv_obj_set_style_transform_pivot_x(label, 0, 0);
        lv_obj_set_style_transform_pivot_y(label, 0, 0);
        if(font_size == 0) {
            lv_obj_set_width(label, 343);
            lv_obj_set_height(label, toolbar_visible ? 187 : 224);
        }
    }

    if(!toolbar_visible)
        return;
    toolbar = crazypod_ui_widget_box(
        page, 0, 206, 320, 34, 0,
        theme == 3 ? 0xFFFFFF : 0x000000,
        theme == 3 ? 24 : 15);
    label = crazypod_ui_widget_label(toolbar, LV_SYMBOL_LEFT,
                       &lv_font_montserrat_16,
                       ink_color, 155);
    lv_obj_set_pos(label, 47, 6);
    label = crazypod_ui_widget_label(
        toolbar,
        LV_SYMBOL_LIST,
        &lv_font_montserrat_12,
        ink_color, 155);
    lv_obj_set_pos(label, 151, 7);
    label = crazypod_ui_widget_label(toolbar, LV_SYMBOL_RIGHT,
                       &lv_font_montserrat_16,
                       ink_color, 155);
    lv_obj_set_pos(label, 255, 6);
    snprintf(progress, sizeof(progress), "%u%%", percent);
    label = crazypod_ui_widget_label(toolbar, progress, &lv_font_montserrat_8,
                       ink_color, 115);
    lv_obj_set_width(label, 42);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 139, 23);
}

void crazypod_books_screen_render_stats(lv_obj_t *content)
{
    lv_obj_t *label;
    lv_obj_t *panel;
    char text[160];

    label = crazypod_ui_widget_label(content, CP_TR("READING STATS"),
                       &lv_font_montserrat_16,
                       CRAZYPOD_BOOKS_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 14, 43);
    panel = crazypod_ui_widget_box(content, 14, 74, 292, 126, 10,
                     CRAZYPOD_BOOKS_PANEL, 220);
    snprintf(text, sizeof(text),
             CP_FMT("%d books\n%d recently opened\n%d favorites\n\n"
                    "Progress is stored on this iPod."),
             crazypod_books_count(),
             crazypod_books_recent_count(),
             crazypod_books_favorite_count());
    label = crazypod_ui_widget_label(panel, text, CRAZYPOD_BOOKS_FONT,
                       CRAZYPOD_BOOKS_WHITE, 230);
    lv_obj_set_pos(label, 14, 13);
    lv_obj_set_width(label, 264);
}

void crazypod_books_screen_render_info(lv_obj_t *content,
                                       int book_index)
{
    const struct crazypod_book *book;
    lv_obj_t *label;
    lv_obj_t *panel;
    char text[256];
    const char *format;

    crazypod_book_probe(book_index);
    book = crazypod_book_get(book_index);
    format = book == NULL ? "" :
        book->format == CRAZYPOD_BOOK_TXT ? CP_TR("TXT") :
        book->format == CRAZYPOD_BOOK_MARKDOWN ? CP_TR("Markdown") : CP_TR("EPUB");

    label = crazypod_ui_widget_label(content, CP_TR("BOOK INFO"),
                       &lv_font_montserrat_16,
                       CRAZYPOD_BOOKS_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 14, 43);
    panel = crazypod_ui_widget_box(content, 14, 74, 292, 132, 10,
                     CRAZYPOD_BOOKS_PANEL, 220);
    snprintf(text, sizeof(text),
             CP_FMT("%.60s\n%s%.60s%s\n%.8s · %lu KB\n\n%.80s"),
             book != NULL ? book->title : CP_FMT("Missing Book"),
             book != NULL && book->author[0] != '\0' ? CP_FMT("by ") : "",
             book != NULL ? book->author : "",
             book != NULL && book->author[0] != '\0' ? "" : CP_FMT("Unknown author"),
             format,
             (unsigned long)(book != NULL ? book->size / 1024u : 0),
             book != NULL ? book->path : "");
    label = crazypod_ui_widget_label(panel, text, CRAZYPOD_BOOKS_FONT,
                       CRAZYPOD_BOOKS_WHITE, 225);
    lv_obj_set_pos(label, 12, 10);
    lv_obj_set_width(label, 268);
    lv_obj_set_height(label, 112);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
}

static void format_clock(char *text, size_t size, uint32_t ms)
{
    uint32_t seconds = ms / 1000u;

    if(seconds >= 3600u)
        snprintf(text, size, "%lu:%02lu:%02lu",
                 (unsigned long)(seconds / 3600u),
                 (unsigned long)(seconds / 60u % 60u),
                 (unsigned long)(seconds % 60u));
    else
        snprintf(text, size, "%lu:%02lu",
                 (unsigned long)(seconds / 60u),
                 (unsigned long)(seconds % 60u));
}

void crazypod_books_screen_render_audiobook(
    lv_obj_t *content, int audiobook_index)
{
    const struct crazypod_audiobook *book;
    const struct crazypod_audiobook_chapter *chapter = NULL;
    lv_obj_t *label;
    lv_obj_t *panel;
    char text[128];
    char elapsed[16];
    char remaining[16];
    uint32_t position;
    uint32_t length;
    int chapter_count;
    int chapter_index = -1;
    int fill;
    bool playing;

    crazypod_audiobook_probe(audiobook_index);
    book = crazypod_audiobook_get(audiobook_index);
    position = crazypod_audiobook_position_ms(audiobook_index);
    length = book != NULL ? book->length_ms : 0;
    chapter_count = crazypod_audiobook_chapter_count(audiobook_index);
    if(chapter_count > 0) {
        chapter_index = crazypod_audiobook_chapter_at(
            audiobook_index, position);
        chapter = crazypod_audiobook_chapter_get(
            audiobook_index, chapter_index);
    }
    playing = crazypod_audiobook_is_playing(audiobook_index);

    label = crazypod_ui_widget_label(
        content, book != NULL ? book->title : CP_TR("AUDIOBOOK"),
        &lv_font_montserrat_16, CRAZYPOD_BOOKS_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 14, 40);
    lv_obj_set_width(label, 292);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    label = crazypod_ui_widget_label(
        content,
        book != NULL && book->author[0] != '\0'
            ? book->author : CP_TR("Unknown author"),
        CRAZYPOD_BOOKS_FONT, CRAZYPOD_BOOKS_WHITE, 170);
    lv_obj_set_pos(label, 14, 64);
    lv_obj_set_width(label, 292);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);

    panel = crazypod_ui_widget_box(content, 14, 92, 292, 114, 10,
                                   CRAZYPOD_BOOKS_PANEL, 220);
    if(chapter != NULL) {
        snprintf(text, sizeof(text), CP_FMT("Chapter %d of %d"),
                 chapter_index + 1, chapter_count);
        label = crazypod_ui_widget_label(
            panel, text, CRAZYPOD_BOOKS_FONT, CRAZYPOD_BOOKS_WHITE, 170);
        lv_obj_set_pos(label, 12, 8);
        label = crazypod_ui_widget_label(
            panel, chapter->title, CRAZYPOD_BOOKS_FONT,
            CRAZYPOD_BOOKS_WHITE, LV_OPA_COVER);
        lv_obj_set_pos(label, 12, 26);
        lv_obj_set_width(label, 268);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    }
    else {
        label = crazypod_ui_widget_label(
            panel, CP_TR("No chapters"), CRAZYPOD_BOOKS_FONT,
            CRAZYPOD_BOOKS_WHITE, 170);
        lv_obj_set_pos(label, 12, 8);
    }

    crazypod_ui_widget_box(panel, 12, 54, 268, 6, LV_RADIUS_CIRCLE,
                           CRAZYPOD_BOOKS_WHITE, 40);
    fill = length > 0
        ? (int)((uint64_t)position * 268u / length) : 0;
    if(fill < 4)
        fill = 4;
    if(fill > 268)
        fill = 268;
    crazypod_ui_widget_box(panel, 12, 54, fill, 6,
                           LV_RADIUS_CIRCLE, 0xD4B46A, 230);

    format_clock(elapsed, sizeof(elapsed), position);
    format_clock(remaining, sizeof(remaining),
                 length > position ? length - position : 0);
    label = crazypod_ui_widget_label(
        panel, elapsed, CRAZYPOD_BOOKS_FONT, CRAZYPOD_BOOKS_WHITE, 200);
    lv_obj_set_pos(label, 12, 66);
    snprintf(text, sizeof(text), "-%s", remaining);
    label = crazypod_ui_widget_label(
        panel, text, CRAZYPOD_BOOKS_FONT, CRAZYPOD_BOOKS_WHITE, 200);
    lv_obj_set_pos(label, 190, 66);
    lv_obj_set_width(label, 90);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);

    snprintf(text, sizeof(text), "%s  %s",
             playing ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE,
             playing ? CP_FMT("Playing") : CP_FMT("Paused"));
    label = crazypod_ui_widget_label(
        panel, text, CRAZYPOD_BOOKS_FONT, CRAZYPOD_BOOKS_WHITE, 200);
    lv_obj_set_pos(label, 12, 88);
    label = crazypod_ui_widget_label(
        panel,
        chapter_count > 0
            ? CP_TR("Left and Right skip chapters")
            : CP_TR("Left and Right skip 30 seconds"),
        CRAZYPOD_BOOKS_FONT, CRAZYPOD_BOOKS_WHITE, 120);
    lv_obj_set_pos(label, 100, 88);
    lv_obj_set_width(label, 180);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
}
