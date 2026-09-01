#include "config.h"

#include "../../../crazypod_l10n.h"

#include <stdio.h>

#include "lvgl.h"

#include "../../../crazypod_books.h"
#include "../../../crazypod_book_image.h"
#include "../../../crazypod_runtime_font.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_books_screen.h"

#define CRAZYPOD_BOOKS_FONT (&lv_font_source_han_sans_sc_14_cjk)
#define CRAZYPOD_BOOKS_READER_TOP 28
#define CRAZYPOD_BOOKS_READER_TOOLBAR_TOP 206
#define CRAZYPOD_BOOKS_READER_MARGIN 4
#define CRAZYPOD_BOOKS_IMAGE_MARGIN 8
#define CRAZYPOD_BOOKS_IMAGE_MAX_HEIGHT 204
#define CRAZYPOD_BOOKS_WHITE 0xFFFFFF
#define CRAZYPOD_BOOKS_PANEL 0x1B1B22

static const lv_font_t *books_text_font(unsigned size)
{
    const lv_font_t *font = crazypod_runtime_font_at_size(size);

    if(font != NULL)
        return font;
    return size >= 16
        ? &lv_font_source_han_sans_sc_16_cjk
        : CRAZYPOD_BOOKS_FONT;
}

void crazypod_books_screen_render_reader(
    lv_obj_t *content, int book_index, uint32_t page_offset,
    const char *page_text, uint32_t page_color, uint32_t ink_color,
    bool toolbar_visible)
{
    const struct crazypod_book *book =
        crazypod_book_get(book_index);
    int theme = crazypod_books_theme();
    int font_size = crazypod_books_font_size();
    const lv_font_t *reader_font = books_text_font(
        font_size == 0 ? 12 : font_size == 2 ? 16 : 14);
    lv_obj_t *page;
    lv_obj_t *toolbar;
    lv_obj_t *label;
    lv_obj_t *image;
    const lv_image_dsc_t *reader_image;
    int reader_height = toolbar_visible
        ? CRAZYPOD_BOOKS_READER_TOOLBAR_TOP - CRAZYPOD_BOOKS_READER_TOP
        : LCD_HEIGHT - CRAZYPOD_BOOKS_READER_TOP;
    int image_decode_height = reader_height >
        CRAZYPOD_BOOKS_IMAGE_MAX_HEIGHT
            ? CRAZYPOD_BOOKS_IMAGE_MAX_HEIGHT : reader_height;
    char progress[24];
    uint32_t total = book != NULL && book->content_size > 0
        ? book->content_size : book != NULL ? book->size : 0;
    unsigned percent = total > 0
        ? page_offset * 100u / total : 0;

    page = crazypod_ui_widget_box(content, 0, 0,
                    LCD_WIDTH, LCD_HEIGHT, 0,
                    page_color, LV_OPA_COVER);
    reader_image = crazypod_book_image_get(
        book_index, page_offset,
        LCD_WIDTH - CRAZYPOD_BOOKS_IMAGE_MARGIN * 2,
        image_decode_height);
    if(reader_image != NULL) {
        uint32_t scale_x = (uint32_t)(LCD_WIDTH -
            CRAZYPOD_BOOKS_IMAGE_MARGIN * 2) *
            LV_SCALE_NONE / reader_image->header.w;
        uint32_t scale_y = (uint32_t)reader_height * LV_SCALE_NONE /
            reader_image->header.h;
        uint32_t scale = scale_x < scale_y ? scale_x : scale_y;
        int width;
        int height;

        if(scale > LV_SCALE_NONE)
            scale = LV_SCALE_NONE;
        if(scale == 0)
            scale = 1;
        width = reader_image->header.w * scale / LV_SCALE_NONE;
        height = reader_image->header.h * scale / LV_SCALE_NONE;
        image = lv_image_create(page);
        lv_image_set_src(image, reader_image);
        lv_image_set_pivot(image, 0, 0);
        lv_image_set_scale(image, scale);
        lv_obj_set_pos(
            image, CRAZYPOD_BOOKS_IMAGE_MARGIN +
            ((LCD_WIDTH - CRAZYPOD_BOOKS_IMAGE_MARGIN * 2) - width) / 2,
            CRAZYPOD_BOOKS_READER_TOP +
            (reader_height - height) / 2);
        lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
    }
    else {
        label = crazypod_ui_widget_label(
            page,
            page_text[0] != '\0'
                ? page_text : CP_TR("This book could not be decoded."),
            reader_font, ink_color,
            LV_OPA_COVER);
        lv_obj_set_pos(label, CRAZYPOD_BOOKS_READER_MARGIN,
                       CRAZYPOD_BOOKS_READER_TOP);
        lv_obj_set_width(label,
                         LCD_WIDTH - CRAZYPOD_BOOKS_READER_MARGIN * 2);
        lv_obj_set_height(
            label,
            (toolbar_visible ? CRAZYPOD_BOOKS_READER_TOOLBAR_TOP : LCD_HEIGHT) -
            CRAZYPOD_BOOKS_READER_TOP);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    }

    if(!toolbar_visible)
        return;
    toolbar = crazypod_ui_widget_box(
        page, 0, CRAZYPOD_BOOKS_READER_TOOLBAR_TOP,
        LCD_WIDTH, LCD_HEIGHT - CRAZYPOD_BOOKS_READER_TOOLBAR_TOP, 0,
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
    label = crazypod_ui_widget_label(panel, text, books_text_font(14),
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
    label = crazypod_ui_widget_label(panel, text, books_text_font(14),
                       CRAZYPOD_BOOKS_WHITE, 225);
    lv_obj_set_pos(label, 12, 10);
    lv_obj_set_width(label, 268);
    lv_obj_set_height(label, 112);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
}
