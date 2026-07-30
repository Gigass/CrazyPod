#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>

#include "lvgl.h"

#include "../../../crazypod_books.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "../../presentation/crazypod_menu_preview_motion.h"
#include "crazypod_book_preview_cover.h"
#include "../../presentation/crazypod_preview_primitives.h"
#include "crazypod_books_feature.h"
#include "crazypod_books_preview.h"

#define COLOR_WHITE 0xFFFFFF

static lv_obj_t *make_box(
    lv_obj_t *parent, int x, int y, int width, int height,
    int radius, uint32_t color, lv_opa_t opacity)
{
    return crazypod_ui_widget_box(
        parent, x, y, width, height, radius, color, opacity);
}

static lv_obj_t *make_label(
    lv_obj_t *parent, const char *text, const lv_font_t *font,
    uint32_t color, lv_opa_t opacity)
{
    return crazypod_ui_widget_label(
        parent, text, font, color, opacity);
}

static void make_pixel_heart(
    lv_obj_t *parent, int x, int y, int unit,
    uint32_t color, lv_opa_t opacity)
{
    crazypod_ui_widget_pixel_heart(
        parent, x, y, unit, color, opacity);
}

static bool books_has_continue(void)
{
    int index = crazypod_books_recent_index();
    const struct crazypod_book *book = crazypod_book_get(index);

    return book != NULL && book->progress > 0;
}

static int books_route_book_index(
    const struct route_state *state, int position)
{
    if(state->route == BOOKS_ROUTE_LIBRARY)
        return position;
    if(state->route == BOOKS_ROUTE_RECENTS)
        return crazypod_books_recent_at(position);
    if(state->route == BOOKS_ROUTE_FAVORITES)
        return crazypod_books_favorite_at(position);
    return state->group;
}

static void render_books_menu_stage(
    lv_obj_t *parent, const struct route_state *state,
    const struct crazypod_book **selected_book,
    const char **detail)
{
    bool has_continue = books_has_continue();
    int logical = state->selected - (has_continue ? 1 : 0);
    int count = crazypod_books_count();
    lv_obj_t *stage;
    lv_obj_t *part;
    lv_obj_t *label;
    int i;

    if(has_continue && state->selected == 0) {
        int index = crazypod_books_recent_index();
        const struct crazypod_book *book = crazypod_book_get(index);
        lv_obj_t *left_page;
        lv_obj_t *right_page;
        *selected_book = book;
        *detail = CP_TR("Resume saved position");
        crazypod_preview_make_plinth(
            parent, 184, 155, 112, 0xAA8B61, 0x2A1B12);
        stage = make_box(parent, 185, 71, 110, 77, 5,
                         0x5B3A25, LV_OPA_COVER);
        lv_obj_set_style_border_width(stage, 1, 0);
        lv_obj_set_style_border_color(
            stage, lv_color_hex(0x9A744C), 0);
        lv_obj_set_style_border_opa(stage, 140, 0);
        crazypod_preview_add_bevel(
            stage, 110, 77, 0xB58B5F, 0x21140C);
        left_page = make_box(stage, 5, 5, 49, 66, 3,
                             0xEFE2C5, LV_OPA_COVER);
        right_page = make_box(stage, 56, 5, 49, 66, 3,
                              0xF7EBD2, LV_OPA_COVER);
        crazypod_preview_add_bevel(
            left_page, 49, 66, 0xFFFFFF, 0x9E8A68);
        crazypod_preview_add_bevel(
            right_page, 49, 66, 0xFFFFFF, 0x9E8A68);
        crazypod_preview_add_paper_rules(
            left_page, 49, 15, 4, 10, 0x6E5946);
        crazypod_preview_add_paper_rules(
            right_page, 49, 15, 4, 10, 0x6E5946);
        make_box(stage, 52, 5, 5, 66, 2,
                 0xB69565, 190);
        make_box(stage, 54, 7, 1, 62, 0,
                 0x5A3821, 145);
        part = make_box(right_page, 37, 0, 7, 31, 1,
                        0xB43B45, 225);
        make_box(part, 1, 0, 2, 31, 0,
                 0xE77780, 95);
        crazypod_menu_preview_motion_register(
            stage, 0, 11, 225, 0, 0, 0, 250,
            0, 8, 225, 0);
        crazypod_menu_preview_motion_register(
            left_page, 22, 0, 182, 120, 0, 30, 280,
            20, 0, 182, 120);
        crazypod_menu_preview_motion_register(
            right_page, -22, 0, 182, -120, 0, 30, 280,
            -20, 0, 182, -120);
        crazypod_menu_preview_motion_register(
            part, 0, -14, 210, 0, 0, 90, 220,
            0, -9, 210, 0);
        return;
    }

    if(logical == 0) {
        int recent_count = crazypod_books_recent_count();
        *detail = recent_count > 0
            ? CP_TR("Latest reading activity") : CP_TR("No recent reading");
        crazypod_preview_make_plinth(
            parent, 181, 152, 118, 0xB38A5E, 0x2C1C12);
        for(i = 2; i >= 0; --i) {
            const struct crazypod_book *book =
                i < recent_count
                    ? crazypod_book_get(
                          crazypod_books_recent_at(i))
                    : NULL;
            stage = crazypod_book_preview_cover_create(
                parent, book,
                186 + i * 13, 68 - i * 5,
                70, 86);
            crazypod_menu_preview_motion_register(
                stage, (i - 1) * 20, 14 + i * 3, 195,
                (i - 1) * 45, 0, (2 - i) * 35, 270,
                (i - 1) * 18, 9, 195,
                (i - 1) * 55);
        }
        return;
    }

    if(logical == 1) {
        *detail = count > 0
            ? CP_TR("Browse the local library") : CP_TR("Import EPUB, TXT or MD");
        make_box(parent, 183, 70, 114, 83, 4,
                 0x261A12, 82);
        stage = crazypod_preview_make_plinth(
            parent, 178, 151, 124, 0xB47A3C, 0x3A2416);
        crazypod_preview_add_fastener(stage, 5, 2, 0x9A7655);
        crazypod_preview_add_fastener(stage, 114, 2, 0x9A7655);
        crazypod_menu_preview_motion_register(
            stage, 0, 16, 230, 0, 0, 0, 220,
            0, 12, 230, 0);
        for(i = 0; i < 4; ++i) {
            const struct crazypod_book *book =
                i < count ? crazypod_book_get(i) : NULL;
            part = crazypod_book_preview_cover_create(
                parent, book, 188 + i * 27,
                75 + (i % 2) * 7,
                21, 76 - (i % 2) * 7);
            crazypod_menu_preview_motion_register(
                part, 0, 44 + (i % 2) * 7, 218,
                (i - 2) * 12, 0, i * 35, 260,
                0, 35, 218, (i - 2) * -12);
        }
        return;
    }

    if(logical == 2) {
        int favorite_count = crazypod_books_favorite_count();
        *detail = favorite_count > 0
            ? CP_TR("Your favorite books") : CP_TR("No favorites yet");
        if(favorite_count > 0) {
            const struct crazypod_book *book =
                crazypod_book_get(
                    crazypod_books_favorite_at(0));
            *selected_book = book;
            crazypod_preview_make_plinth(
                parent, 184, 155, 112, 0xB58D63, 0x301D13);
            part = crazypod_book_preview_cover_create(
                parent, book, 204, 55, 72, 101);
            stage = make_box(parent, 257, 52, 26, 26,
                             LV_RADIUS_CIRCLE, 0x6D1526, 240);
            lv_obj_set_style_border_width(stage, 1, 0);
            lv_obj_set_style_border_color(
                stage, lv_color_hex(0xD7B06A), 0);
            lv_obj_set_style_border_opa(stage, 155, 0);
            crazypod_preview_add_bevel(
                stage, 26, 26, 0xF0CE86, 0x2D0710);
            make_pixel_heart(stage, 5, 7, 2,
                             0xF7D788, LV_OPA_COVER);
            crazypod_menu_preview_motion_register(
                part, 0, 21, 210, -25, 0, 0, 270,
                0, 16, 210, 25);
            crazypod_menu_preview_motion_register(
                stage, 12, -13, 165, 120, 0, 70, 240,
                10, -10, 165, 170);
        }
        else {
            stage = make_box(parent, 184, 70, 112, 84, 9,
                             0x5B0F19, LV_OPA_COVER);
            lv_obj_set_style_bg_grad_color(
                stage, lv_color_hex(0x250408), 0);
            lv_obj_set_style_bg_grad_dir(
                stage, LV_GRAD_DIR_VER, 0);
            lv_obj_set_style_border_width(stage, 1, 0);
            lv_obj_set_style_border_color(
                stage, lv_color_hex(0xE8C875), 0);
            lv_obj_set_style_border_opa(stage, 48, 0);
            crazypod_preview_add_bevel(
                stage, 112, 84, 0xA84854, 0x120103);
            crazypod_preview_add_fastener(
                stage, 7, 7, 0xA77D4C);
            crazypod_preview_add_fastener(
                stage, 100, 7, 0xA77D4C);
            make_pixel_heart(stage, 40, 27, 4,
                             0xE8C875, 88);
            make_box(stage, 30, 67, 52, 4, 2,
                     0xD1AE63, 75);
            crazypod_menu_preview_motion_register(
                stage, 0, 20, 208, 0, 0, 0, 260,
                0, 14, 208, 0);
        }
        return;
    }

    if(logical == 3) {
        char value[16];
        *detail = CP_TR("Library and progress totals");
        stage = make_box(parent, 184, 61, 112, 94, 10,
                         0x15110C, 232);
        lv_obj_set_style_border_width(stage, 1, 0);
        lv_obj_set_style_border_color(
            stage, lv_color_hex(0xD4B46A), 0);
        lv_obj_set_style_border_opa(stage, 82, 0);
        crazypod_preview_add_bevel(
            stage, 112, 94, 0xA58D5B, 0x000000);
        crazypod_preview_add_fastener(stage, 5, 5, 0xA89265);
        crazypod_preview_add_fastener(stage, 102, 5, 0xA89265);
        make_box(stage, 55, 13, 1, 48, 0,
                 0xD4B46A, 55);
        snprintf(value, sizeof(value), CP_FMT("%d"), count);
        label = make_label(stage, value,
                           &lv_font_montserrat_24,
                           0xF6D58C, LV_OPA_COVER);
        lv_obj_set_width(label, 52);
        lv_obj_set_style_text_align(
            label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 4, 14);
        label = make_label(stage, CP_TR("BOOKS"),
                           &lv_font_montserrat_8,
                           COLOR_WHITE, 110);
        lv_obj_set_width(label, 52);
        lv_obj_set_style_text_align(
            label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 4, 44);
        snprintf(value, sizeof(value), CP_FMT("%d"),
                 crazypod_books_favorite_count());
        label = make_label(stage, value,
                           &lv_font_montserrat_24,
                           0xF6D58C, LV_OPA_COVER);
        lv_obj_set_width(label, 52);
        lv_obj_set_style_text_align(
            label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 56, 14);
        label = make_label(stage, CP_TR("FAVORITES"),
                           &lv_font_montserrat_8,
                           COLOR_WHITE, 110);
        lv_obj_set_width(label, 52);
        lv_obj_set_style_text_align(
            label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 56, 44);
        make_box(stage, 12, 67, 88, 5,
                 LV_RADIUS_CIRCLE, COLOR_WHITE, 30);
        make_box(stage, 12, 67,
                 count > 0 ? 56 : 4, 5,
                 LV_RADIUS_CIRCLE, 0xD4B46A, 215);
        make_box(stage, 12, 77, 88, 1, 0,
                 0xD4B46A, 52);
        make_box(stage, 12, 83,
                 crazypod_books_favorite_count() > 0 ? 34 : 4,
                 3, 1, 0x8E7447, 165);
        crazypod_menu_preview_motion_register(
            stage, 13, 0, 220, 30, 0, 0, 270,
            12, 0, 220, 45);
        return;
    }

    *detail = CP_TR("Font, size and page theme");
    stage = make_box(parent, 193, 57, 94, 101, 6,
                     0xF4E9CF, LV_OPA_COVER);
    lv_obj_set_style_border_width(stage, 1, 0);
    lv_obj_set_style_border_color(
        stage, lv_color_hex(0xD5BB84), 0);
    lv_obj_set_style_border_opa(stage, 150, 0);
    crazypod_preview_add_bevel(
        stage, 94, 101, 0xFFFFFF, 0x95815F);
    make_box(stage, 7, 7, 80, 4, 2,
             0xB89B68, 95);
    label = make_label(stage, CP_TR("Aa"),
                       &lv_font_montserrat_24,
                       0x4A3524, LV_OPA_COVER);
    lv_obj_set_pos(label, 31, 17);
    crazypod_preview_add_paper_rules(
        stage, 94, 55, 3, 10, 0x665344);
    {
        static const uint32_t colors[] = {
            0xE8D7B7, 0xF7F7F4, 0xCFE6D8, 0x242424
        };
        for(i = 0; i < 4; ++i) {
            lv_obj_t *swatch = make_box(
                stage, 18 + i * 16, 84,
                11, 11, LV_RADIUS_CIRCLE,
                colors[i], LV_OPA_COVER);
            if(i == crazypod_books_theme()) {
                lv_obj_set_style_border_width(swatch, 2, 0);
                lv_obj_set_style_border_color(
                    swatch, lv_color_hex(0x9A6A2C), 0);
                lv_obj_set_style_border_opa(
                    swatch, LV_OPA_COVER, 0);
            }
        }
    }
    crazypod_menu_preview_motion_register(
        stage, 0, 18, 220, 0, 0, 0, 270,
        -8, 12, 220, -20);
}

static void render_books_settings_stage(
    lv_obj_t *parent, const struct route_state *state,
    const char **detail)
{
    lv_obj_t *page;
    lv_obj_t *label;
    int selected = state->selected;
    int i;

    if(selected == 2) {
        page = make_box(parent, 198, 67, 84, 84, 18,
                        0xA56D2E, LV_OPA_COVER);
        label = make_label(page, LV_SYMBOL_REFRESH,
                           &lv_font_montserrat_24,
                           COLOR_WHITE, 235);
        lv_obj_center(label);
        *detail = CP_TR("Scan Books folders again");
        return;
    }

    {
        static const uint32_t page_colors[] = {
            0xE8D7B7, 0xF7F7F4, 0xCFE6D8, 0x242424
        };
        int theme = crazypod_books_theme();
        uint32_t ink = theme == 3 ? 0xF2F2EE : 0x4A3524;
        page = make_box(parent, 194, 57, 92, 101, 6,
                        page_colors[theme], LV_OPA_COVER);
        lv_obj_set_style_border_width(page, 1, 0);
        lv_obj_set_style_border_color(
            page, lv_color_hex(0xD5BB84), 0);
        lv_obj_set_style_border_opa(page, 135, 0);
        label = make_label(
            page, CP_TR("Aa"),
            crazypod_books_font_size() == 0
                ? &lv_font_montserrat_16
                : &lv_font_montserrat_24,
            ink, LV_OPA_COVER);
        lv_obj_set_width(label, 92);
        lv_obj_set_style_text_align(
            label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(label, 0,
                       crazypod_books_font_size() == 0 ? 17 : 11);
        for(i = 0; i < 3; ++i)
            make_box(page, 17, 54 + i * 10,
                     58 - i * 7, 2, 1,
                     ink, 82);
        if(selected == 1) {
            static const uint32_t swatch_colors[] = {
                0xE8D7B7, 0xF7F7F4, 0xCFE6D8, 0x242424
            };
            for(i = 0; i < 4; ++i)
                make_box(page, 15 + i * 17, 84,
                         12, 12, LV_RADIUS_CIRCLE,
                         swatch_colors[i],
                         i == theme ? 255 : 115);
        }
    }
    *detail = selected == 0
        ? CP_TR("Choose size in a focused popup")
        : CP_TR("Choose a page theme in a popup");
}

void crazypod_books_preview_render(
    lv_obj_t *parent, const struct route_state *state,
    const lv_font_t *metadata_font)
{
    int index = books_route_book_index(state, state->selected);
    const struct crazypod_book *book;
    const char *title = NULL;
    const char *detail = "";
    lv_obj_t *label;
    lv_obj_t *text_panel;
    char detail_text[64];

    if(index >= 0)
        crazypod_book_probe(index);
    book = crazypod_book_get(index);
    crazypod_books_feature_item_title(
        state, state->selected, &title);

    if(state->route == BOOKS_ROUTE_MENU) {
        book = NULL;
        render_books_menu_stage(
            parent, state, &book, &detail);
    }
    else if(state->route == BOOKS_ROUTE_READING_SETTINGS) {
        book = NULL;
        render_books_settings_stage(parent, state, &detail);
    }
    else {
        make_box(parent, 182, 157, 116, 8,
                 LV_RADIUS_CIRCLE, 0x000000, 66);
        crazypod_book_preview_cover_create(parent, book, 204, 55, 72, 101);
        if(book != NULL &&
           (book->content_size > 0 || book->size > 0)) {
            uint32_t total = book->content_size > 0
                ? book->content_size : book->size;
            snprintf(detail_text, sizeof(detail_text),
                     CP_FMT("%lu%% read%s"),
                     (unsigned long)(
                         book->progress * 100u / total),
                     book->favorite ? CP_FMT(" · Favorite") : "");
            detail = detail_text;
        }
        else {
            snprintf(detail_text, sizeof(detail_text),
                     CP_FMT("%d book%s"), crazypod_books_count(),
                     crazypod_books_count() == 1 ? "" : "s");
            detail = detail_text;
        }

        if(state->route == BOOKS_ROUTE_ACTIONS) {
            static const char *const symbols[] = {
                LV_SYMBOL_PLAY, LV_SYMBOL_FILE, LV_SYMBOL_LIST,
                LV_SYMBOL_OK, LV_SYMBOL_SETTINGS, LV_SYMBOL_TRASH
            };
            static const uint32_t colors[] = {
                0x34C759, 0x4F9BFF, 0xFFB340,
                0xD4B46A, 0x8E8E93, 0xFF453A
            };
            lv_obj_t *badge = make_box(
                parent, 255, 51, 29, 29,
                LV_RADIUS_CIRCLE,
                colors[state->selected],
                LV_OPA_COVER);
            label = make_label(
                badge, symbols[state->selected],
                &lv_font_montserrat_10,
                COLOR_WHITE, LV_OPA_COVER);
            lv_obj_center(label);
            detail = state->selected == 0
                ? CP_TR("Open at saved position")
                : state->selected == 1
                    ? CP_TR("Open the saved bookmark")
                    : state->selected == 2
                        ? CP_TR("Browse EPUB chapters")
                        : state->selected == 3
                            ? CP_TR("Change favorite status")
                            : state->selected == 4
                                ? CP_TR("View file information")
                                : CP_TR("Delete this local file");
        }
        else if(state->route == BOOKS_ROUTE_CHAPTERS) {
            snprintf(detail_text, sizeof(detail_text),
                     CP_FMT("Chapter %d of %d"),
                     state->selected + 1,
                     crazypod_book_chapter_count(state->group));
            detail = detail_text;
        }
        else if(state->route == BOOKS_ROUTE_BOOKMARKS)
            detail = CP_TR("Jump to the saved page");
        else if(state->route == BOOKS_ROUTE_DELETE_CONFIRM)
            detail = CP_TR("Hold center to delete permanently");
    }

    if(state->route != BOOKS_ROUTE_MENU && book != NULL &&
       state->route != BOOKS_ROUTE_ACTIONS &&
       state->route != BOOKS_ROUTE_CHAPTERS &&
       state->route != BOOKS_ROUTE_BOOKMARKS &&
       state->route != BOOKS_ROUTE_DELETE_CONFIRM)
        title = book->title;
    else if(state->route == BOOKS_ROUTE_MENU &&
            book != NULL && books_has_continue() &&
            state->selected == 0)
        title = book->title;

    text_panel = crazypod_preview_make_text_panel(parent, 172, 50);
    label = make_label(text_panel,
                       title != NULL ? title : CP_TR("Books"),
                       metadata_font,
                       COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 7, 5);
    label = make_label(text_panel, detail,
                       &lv_font_montserrat_8,
                       COLOR_WHITE, 135);
    lv_obj_set_width(label, 126);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(label, 7, 30);
    if(state->route == BOOKS_ROUTE_MENU)
        crazypod_menu_preview_motion_register(
            text_panel, 0, 9, 246, 0, 0, 70, 220,
            0, 6, 246, 0);
}



#endif
