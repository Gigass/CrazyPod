#include "config.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "crazypod_choice_overlay.h"
#include "crazypod_ui_widgets.h"

#define CHOICE_ROWS 5
#define COLOR_PANEL 0x1B1B22
#define COLOR_WHITE 0xFFFFFF
#define POPUP_X 35
#define POPUP_Y 32
#define POPUP_WIDTH 250
#define POPUP_HEIGHT 176

struct choice_overlay_view {
    int kind;
    int id;
    int selected;
    int count;
    lv_obj_t *root;
    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *value;
    lv_obj_t *counter;
    lv_obj_t *rows[CHOICE_ROWS];
    lv_obj_t *swatches[CHOICE_ROWS];
    lv_obj_t *labels[CHOICE_ROWS];
    lv_obj_t *markers[CHOICE_ROWS];
    lv_obj_t *scroll_thumb;
    const lv_font_t *metadata_font;
    struct crazypod_choice_overlay_callbacks callbacks;
};

static struct choice_overlay_view view;

static void refresh(void)
{
    int start;
    int row;
    char counter[24];

    if(view.root == NULL || view.panel == NULL)
        return;
    view.count = view.callbacks.count(
        view.kind, view.id, view.callbacks.context);
    if(view.count <= 0) {
        view.selected = 0;
        return;
    }
    if(view.selected < 0)
        view.selected = 0;
    if(view.selected >= view.count)
        view.selected = view.count - 1;

    lv_label_set_text(
        view.title,
        view.callbacks.title(
            view.kind, view.id, view.callbacks.context));
    lv_label_set_text(
        view.value,
        view.callbacks.item_title(
            view.kind, view.id, view.selected,
            view.callbacks.context));
    snprintf(counter, sizeof(counter), "%d/%d",
             view.selected + 1, view.count);
    lv_label_set_text(view.counter, counter);

    if(view.count <= CHOICE_ROWS)
        start = 0;
    else {
        start = view.selected - CHOICE_ROWS / 2;
        if(start < 0)
            start = 0;
        if(start > view.count - CHOICE_ROWS)
            start = view.count - CHOICE_ROWS;
    }

    for(row = 0; row < CHOICE_ROWS; ++row) {
        int index = start + row;
        bool selected = index == view.selected;
        bool current;
        uint32_t swatch_color = COLOR_WHITE;
        bool has_color;

        if(index >= view.count) {
            lv_obj_add_flag(view.rows[row], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        current = index == view.callbacks.current_index(
            view.kind, view.id, view.callbacks.context);
        lv_obj_remove_flag(view.rows[row], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(
            view.labels[row],
            view.callbacks.item_title(
                view.kind, view.id, index,
                view.callbacks.context));
        lv_label_set_text(
            view.markers[row],
            current ? LV_SYMBOL_OK :
            selected ? LV_SYMBOL_PLAY : LV_SYMBOL_BULLET);
        lv_obj_set_style_bg_color(
            view.rows[row],
            lv_color_hex(selected ? COLOR_WHITE : COLOR_PANEL), 0);
        lv_obj_set_style_bg_opa(
            view.rows[row], selected ? 32 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(
            view.rows[row], selected ? 1 : 0, 0);
        lv_obj_set_style_border_color(
            view.rows[row], lv_color_hex(COLOR_WHITE), 0);
        lv_obj_set_style_border_opa(
            view.rows[row], selected ? 72 : 0, 0);
        lv_obj_set_style_text_opa(
            view.labels[row], selected ? 255 : 180, 0);
        lv_obj_set_style_text_opa(
            view.markers[row], current ? 235 :
            selected ? 190 : 80, 0);

        has_color = view.callbacks.item_color(
            view.kind, view.id, index, &swatch_color,
            view.callbacks.context);
        lv_obj_set_style_bg_color(
            view.swatches[row],
            lv_color_hex(has_color ? swatch_color : COLOR_WHITE), 0);
        lv_obj_set_style_bg_opa(
            view.swatches[row],
            has_color ? LV_OPA_COVER : selected ? 72 : 28, 0);
    }

    if(view.count > CHOICE_ROWS) {
        const int track_y = 57;
        const int track_height = 92;
        int thumb_height =
            track_height * CHOICE_ROWS / view.count;
        int thumb_y;

        if(thumb_height < 12)
            thumb_height = 12;
        thumb_y = track_y + (track_height - thumb_height) *
            view.selected / (view.count - 1);
        lv_obj_set_y(view.scroll_thumb, thumb_y);
        lv_obj_set_height(view.scroll_thumb, thumb_height);
        lv_obj_remove_flag(
            view.scroll_thumb, LV_OBJ_FLAG_HIDDEN);
    }
    else
        lv_obj_add_flag(view.scroll_thumb, LV_OBJ_FLAG_HIDDEN);
}

void crazypod_choice_overlay_reset(void)
{
    memset(&view, 0, sizeof(view));
}

void crazypod_choice_overlay_dismiss(void)
{
    if(view.root != NULL) {
        lv_anim_delete(view.panel, NULL);
        lv_obj_delete(view.root);
    }
    crazypod_choice_overlay_reset();
}

void crazypod_choice_overlay_show(
    lv_obj_t *parent, int kind, int id, int selected,
    const lv_font_t *metadata_font,
    const struct crazypod_choice_overlay_callbacks *callbacks)
{
    lv_obj_t *track;
    int row;

    if(parent == NULL || metadata_font == NULL ||
       callbacks == NULL || callbacks->count == NULL ||
       callbacks->current_index == NULL ||
       callbacks->title == NULL ||
       callbacks->item_title == NULL ||
       callbacks->item_color == NULL ||
       callbacks->create_panel == NULL)
        return;

    crazypod_choice_overlay_dismiss();
    view.kind = kind;
    view.id = id;
    view.callbacks = *callbacks;
    view.metadata_font = metadata_font;
    view.count = callbacks->count(kind, id, callbacks->context);
    view.selected = selected < 0
        ? callbacks->current_index(kind, id, callbacks->context)
        : selected;
    view.root = crazypod_ui_widget_box(
        parent, 0, 0, LCD_WIDTH, LCD_HEIGHT, 0, 0x000000, 30);
    lv_obj_remove_flag(view.root, LV_OBJ_FLAG_CLICKABLE);
    view.panel = callbacks->create_panel(
        view.root, POPUP_X, POPUP_Y,
        POPUP_WIDTH, POPUP_HEIGHT, callbacks->context);
    if(view.panel == NULL) {
        crazypod_choice_overlay_dismiss();
        return;
    }

    view.title = crazypod_ui_widget_label(
        view.panel, "", &lv_font_montserrat_10,
        COLOR_WHITE, 100);
    lv_obj_set_width(view.title, 170);
    lv_label_set_long_mode(view.title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(view.title, 16, 13);

    view.counter = crazypod_ui_widget_label(
        view.panel, "0/0", &lv_font_montserrat_8,
        COLOR_WHITE, 120);
    lv_obj_set_width(view.counter, 48);
    lv_obj_set_style_text_align(
        view.counter, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(view.counter, 186, 15);

    view.value = crazypod_ui_widget_label(
        view.panel, "", &lv_font_montserrat_8,
        COLOR_WHITE, 170);
    lv_obj_set_width(view.value, 218);
    lv_label_set_long_mode(view.value, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(view.value, 16, 31);

    for(row = 0; row < CHOICE_ROWS; ++row) {
        int y = 52 + row * 23;

        view.rows[row] = crazypod_ui_widget_box(
            view.panel, 12, y, 218, 21, 8,
            COLOR_WHITE, LV_OPA_TRANSP);
        view.swatches[row] = crazypod_ui_widget_box(
            view.rows[row], 10, 6, 9, 9,
            LV_RADIUS_CIRCLE, COLOR_WHITE, 35);
        view.labels[row] = crazypod_ui_widget_label(
            view.rows[row], "", metadata_font,
            COLOR_WHITE, 180);
        lv_obj_set_pos(view.labels[row], 30, 3);
        lv_obj_set_width(view.labels[row], 146);
        lv_obj_set_height(view.labels[row], 15);
        lv_label_set_long_mode(
            view.labels[row], LV_LABEL_LONG_MODE_DOTS);
        view.markers[row] = crazypod_ui_widget_label(
            view.rows[row], LV_SYMBOL_BULLET,
            &lv_font_montserrat_8, COLOR_WHITE, 80);
        lv_obj_set_width(view.markers[row], 24);
        lv_obj_set_style_text_align(
            view.markers[row], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(view.markers[row], 184, 6);
    }

    track = crazypod_ui_widget_box(
        view.panel, 236, 57, 2, 92, 1,
        COLOR_WHITE, 24);
    (void)track;
    view.scroll_thumb = crazypod_ui_widget_box(
        view.panel, 236, 57, 2, 20, 1,
        COLOR_WHITE, 150);

    refresh();
    if(callbacks->animate_panel != NULL)
        callbacks->animate_panel(
            view.panel, POPUP_Y, callbacks->context);
}

void crazypod_choice_overlay_move(int direction)
{
    int next;

    if(!crazypod_choice_overlay_visible() || view.count <= 0)
        return;
    next = view.selected + direction;
    if(next < 0)
        next = 0;
    if(next >= view.count)
        next = view.count - 1;
    if(next == view.selected)
        return;
    view.selected = next;
    refresh();
}

bool crazypod_choice_overlay_visible(void)
{
    return view.kind != 0 && view.root != NULL;
}

int crazypod_choice_overlay_kind(void)
{
    return view.kind;
}

int crazypod_choice_overlay_id(void)
{
    return view.id;
}

int crazypod_choice_overlay_selected(void)
{
    return view.selected;
}

#endif
