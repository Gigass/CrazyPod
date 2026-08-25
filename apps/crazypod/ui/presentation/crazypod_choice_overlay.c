#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "crazypod_choice_overlay.h"
#include "crazypod_popup_layout.h"
#include "crazypod_ui_widgets.h"

#define CHOICE_ROWS 4
#define COLOR_PANEL 0x1B1B22
#define COLOR_WHITE 0xFFFFFF
#define COLOR_SUCCESS 0x34DB7A
#define COLOR_FAILURE 0xFF4D59
#define POPUP_MIN_WIDTH 156
#define POPUP_MAX_WIDTH (LCD_WIDTH - 32)
#define POPUP_ROW_LABEL_CHROME_WIDTH 72
#define POPUP_ROW_HEIGHT 28
#define POPUP_ACTION_ROW_Y 38
#define POPUP_CHOICE_ROW_Y 50
#define POPUP_BOTTOM_PADDING 10

struct choice_overlay_view {
    int kind;
    int id;
    int selected;
    int count;
    bool receipt_visible;
    bool action_layout;
    lv_obj_t *root;
    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *value;
    lv_obj_t *rows[CHOICE_ROWS];
    lv_obj_t *hold_fills[CHOICE_ROWS];
    lv_obj_t *swatches[CHOICE_ROWS];
    lv_obj_t *labels[CHOICE_ROWS];
    lv_obj_t *markers[CHOICE_ROWS];
    lv_obj_t *scroll_track;
    lv_obj_t *scroll_thumb;
    lv_obj_t *receipt_ring;
    lv_obj_t *receipt_symbol;
    lv_obj_t *receipt_label;
    struct crazypod_popup_geometry geometry;
    int row_width;
    int row_y;
    int visible_start;
    bool hold_feedback_active;
    const lv_font_t *metadata_font;
    struct crazypod_choice_overlay_callbacks callbacks;
};

static struct choice_overlay_view view;
static void refresh(void);

static void hold_feedback_anim(void *target, int32_t value)
{
    lv_obj_set_width(target, value);
}

static void stop_hold_feedback(bool restore)
{
    int row;

    if(!view.hold_feedback_active)
        return;
    for(row = 0; row < CHOICE_ROWS; ++row) {
        if(view.hold_fills[row] != NULL) {
            lv_anim_delete(
                view.hold_fills[row], hold_feedback_anim);
            lv_obj_set_width(view.hold_fills[row], 0);
        }
    }
    view.hold_feedback_active = false;
    if(restore)
        refresh();
}

static void receipt_scale_anim(void *target, int32_t value)
{
    lv_obj_set_style_transform_scale(target, value, 0);
}

static void receipt_opa_anim(void *target, int32_t value)
{
    lv_obj_set_style_opa(target, (lv_opa_t)value, 0);
}

static void set_content_hidden(bool hidden)
{
    int row;

    if(view.title == NULL)
        return;
    if(hidden) {
        lv_obj_add_flag(view.title, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view.value, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view.scroll_track, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view.scroll_thumb, LV_OBJ_FLAG_HIDDEN);
        for(row = 0; row < CHOICE_ROWS; ++row)
            lv_obj_add_flag(view.rows[row], LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_remove_flag(view.title, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(view.value, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(view.scroll_track, LV_OBJ_FLAG_HIDDEN);
}

static struct crazypod_popup_geometry calculate_geometry(void)
{
    int content_width;
    int title_width;
    int longest_item = 0;
    int visible_rows;
    int index;
    int width;
    int height;

    title_width = crazypod_popup_text_width(
        view.callbacks.title(
            view.kind, view.id, view.callbacks.context),
        &lv_font_montserrat_10);
    for(index = 0; index < view.count; ++index) {
        int item_width = crazypod_popup_text_width(
            view.callbacks.item_title(
                view.kind, view.id, index,
                view.callbacks.context),
            view.metadata_font);

        if(item_width > longest_item)
            longest_item = item_width;
    }
    content_width = title_width + 28;
    if(longest_item + POPUP_ROW_LABEL_CHROME_WIDTH > content_width)
        content_width = longest_item + POPUP_ROW_LABEL_CHROME_WIDTH;
    width = crazypod_popup_clamp_width(
        content_width, 0,
        POPUP_MIN_WIDTH, POPUP_MAX_WIDTH);
    visible_rows = view.count < CHOICE_ROWS
        ? view.count : CHOICE_ROWS;
    if(visible_rows < 1)
        visible_rows = 1;
    view.row_y = view.action_layout
        ? POPUP_ACTION_ROW_Y : POPUP_CHOICE_ROW_Y;
    height = view.row_y +
        visible_rows * POPUP_ROW_HEIGHT +
        POPUP_BOTTOM_PADDING;
    return crazypod_popup_centered_geometry(width, height);
}

static void refresh(void)
{
    int start;
    int row;
    char value[160];

    if(view.root == NULL || view.panel == NULL ||
       view.receipt_visible)
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

    CP_LV_LABEL_SET_TEXT(
        view.title,
        view.callbacks.title(
            view.kind, view.id, view.callbacks.context));
    if(view.action_layout)
        CP_LV_LABEL_SET_TEXT(view.value, "");
    else {
        snprintf(
            value, sizeof(value), CP_FMT("%s  %d/%d"),
            view.callbacks.item_title(
                view.kind, view.id, view.selected,
                view.callbacks.context),
            view.selected + 1, view.count);
        CP_LV_LABEL_SET_TEXT(view.value, value);
    }

    if(view.count <= CHOICE_ROWS)
        start = 0;
    else {
        start = view.selected - CHOICE_ROWS / 2;
        if(start < 0)
            start = 0;
        if(start > view.count - CHOICE_ROWS)
            start = view.count - CHOICE_ROWS;
    }

    view.visible_start = start;
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
        CP_LV_LABEL_SET_TEXT(
            view.labels[row],
            view.callbacks.item_title(
                view.kind, view.id, index,
                view.callbacks.context));
        CP_LV_LABEL_SET_TEXT(
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
        lv_obj_set_width(view.hold_fills[row], 0);

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
        const int track_y = view.row_y + 3;
        const int track_height =
            CHOICE_ROWS * POPUP_ROW_HEIGHT - 6;
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
        lv_obj_remove_flag(
            view.scroll_track, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_add_flag(view.scroll_thumb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view.scroll_track, LV_OBJ_FLAG_HIDDEN);
    }
}

void crazypod_choice_overlay_reset(void)
{
    memset(&view, 0, sizeof(view));
}

void crazypod_choice_overlay_dismiss(void)
{
    stop_hold_feedback(false);
    if(view.root != NULL) {
        lv_anim_delete(view.panel, NULL);
        if(view.receipt_ring != NULL)
            lv_anim_delete(view.receipt_ring, NULL);
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
       callbacks->action_layout == NULL ||
       callbacks->create_panel == NULL)
        return;

    crazypod_choice_overlay_dismiss();
    view.kind = kind;
    view.id = id;
    view.callbacks = *callbacks;
    view.action_layout = callbacks->action_layout(
        kind, id, callbacks->context);
    view.metadata_font = metadata_font;
    view.count = callbacks->count(kind, id, callbacks->context);
    view.geometry = calculate_geometry();
    view.row_width = view.geometry.width - 24;
    view.selected = selected < 0
        ? callbacks->current_index(kind, id, callbacks->context)
        : selected;
    view.root = crazypod_ui_widget_box(
        parent, 0, 0, LCD_WIDTH, LCD_HEIGHT,
        0, 0x000000, LV_OPA_TRANSP);
    lv_obj_remove_flag(view.root, LV_OBJ_FLAG_CLICKABLE);
    if(callbacks->create_underlay != NULL)
        (void)callbacks->create_underlay(
            view.root, callbacks->context);
    view.panel = callbacks->create_panel(
        view.root, view.geometry.x, view.geometry.y,
        view.geometry.width, view.geometry.height,
        callbacks->context);
    if(view.panel == NULL) {
        crazypod_choice_overlay_dismiss();
        return;
    }

    view.title = crazypod_ui_widget_label(
        view.panel, "", &lv_font_montserrat_10,
        COLOR_WHITE, 100);
    lv_obj_set_width(view.title, view.geometry.width - 24);
    lv_obj_set_style_text_align(
        view.title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(view.title, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(view.title, 12, 12);

    view.value = crazypod_ui_widget_label(
        view.panel, "", &lv_font_montserrat_8,
        COLOR_WHITE, 170);
    lv_obj_set_width(view.value, view.geometry.width - 24);
    lv_obj_set_style_text_align(
        view.value, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(view.value, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_pos(view.value, 12, 29);
    if(view.action_layout)
        lv_obj_add_flag(view.value, LV_OBJ_FLAG_HIDDEN);

    for(row = 0; row < CHOICE_ROWS; ++row) {
        int y = view.row_y + row * POPUP_ROW_HEIGHT;

        view.rows[row] = crazypod_ui_widget_box(
            view.panel, 12, y,
            view.row_width, POPUP_ROW_HEIGHT, 8,
            COLOR_WHITE, LV_OPA_TRANSP);
        view.hold_fills[row] = crazypod_ui_widget_box(
            view.rows[row], 0, 0, 0,
            POPUP_ROW_HEIGHT, 8,
            COLOR_SUCCESS, 68);
        view.swatches[row] = crazypod_ui_widget_box(
            view.rows[row], 9, 9, 9, 9,
            LV_RADIUS_CIRCLE, COLOR_WHITE, 35);
        view.labels[row] = crazypod_ui_widget_label(
            view.rows[row], "", metadata_font,
            COLOR_WHITE, 180);
        lv_obj_set_width(
            view.labels[row], view.row_width - 48);
        lv_obj_set_style_text_align(
            view.labels[row], LV_TEXT_ALIGN_LEFT, 0);
        crazypod_ui_widget_align_row_label(
            view.labels[row], 24, CRAZYPOD_UI_ROW_LABEL_TEXT);
        lv_label_set_long_mode(
            view.labels[row], LV_LABEL_LONG_MODE_DOTS);
        view.markers[row] = crazypod_ui_widget_label(
            view.rows[row], LV_SYMBOL_BULLET,
            &lv_font_montserrat_8, COLOR_WHITE, 80);
        lv_obj_set_width(view.markers[row], 24);
        lv_obj_set_style_text_align(
            view.markers[row], LV_TEXT_ALIGN_CENTER, 0);
        crazypod_ui_widget_align_row_label(
            view.markers[row], view.row_width - 27,
            CRAZYPOD_UI_ROW_LABEL_MARKER);
    }

    track = crazypod_ui_widget_box(
        view.panel, view.geometry.width - 8,
        view.row_y + 3, 2,
        CHOICE_ROWS * POPUP_ROW_HEIGHT - 6, 1,
        COLOR_WHITE, 24);
    view.scroll_track = track;
    view.scroll_thumb = crazypod_ui_widget_box(
        view.panel, view.geometry.width - 8,
        view.row_y + 3, 2, 20, 1,
        COLOR_WHITE, 150);

    refresh();
    if(callbacks->animate_panel != NULL)
        callbacks->animate_panel(
            view.panel, view.geometry.y,
            callbacks->context);
}

void crazypod_choice_overlay_move(int direction)
{
    int next;

    if(!crazypod_choice_overlay_visible() ||
       view.receipt_visible || view.count <= 0)
        return;
    stop_hold_feedback(true);
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

void crazypod_choice_overlay_begin_hold_feedback(int duration_ms)
{
    lv_anim_t animation;
    int row = view.selected - view.visible_start;

    if(!crazypod_choice_overlay_visible() ||
       view.receipt_visible || row < 0 || row >= CHOICE_ROWS ||
       view.hold_fills[row] == NULL || duration_ms <= 0)
        return;
    stop_hold_feedback(true);
    view.hold_feedback_active = true;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, view.hold_fills[row]);
    lv_anim_set_exec_cb(&animation, hold_feedback_anim);
    lv_anim_set_values(&animation, 0, view.row_width);
    lv_anim_set_duration(&animation, duration_ms);
    lv_anim_set_path_cb(&animation, lv_anim_path_linear);
    lv_anim_start(&animation);
}

void crazypod_choice_overlay_cancel_hold_feedback(void)
{
    stop_hold_feedback(true);
}

void crazypod_choice_overlay_show_receipt(
    const char *label, bool success, bool animated)
{
    uint32_t color = success ? COLOR_SUCCESS : COLOR_FAILURE;
    const char *symbol = success ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE;
    const char *resolved_label = label != NULL ? label : "";
    struct crazypod_popup_geometry geometry;
    lv_obj_t *old_panel;
    lv_obj_t *new_panel;
    int label_width;
    int label_height;
    int receipt_y = 10;
    int receipt_size = 48;
    int receipt_gap = 10;
    lv_anim_t animation;

    if(!crazypod_choice_overlay_visible() ||
       view.panel == NULL || view.receipt_visible)
        return;
    stop_hold_feedback(false);
    label_width = crazypod_popup_text_width(
        resolved_label, view.metadata_font);
    geometry = crazypod_popup_centered_geometry(
        crazypod_popup_clamp_width(
            label_width, 22, 132, 208),
        1);
    label_height = crazypod_popup_wrapped_text_height(
        resolved_label, view.metadata_font,
        geometry.width - 20, 1);
    geometry = crazypod_popup_centered_geometry(
        geometry.width,
        receipt_y + receipt_size + receipt_gap +
            label_height + 10);
    old_panel = view.panel;
    set_content_hidden(true);
    lv_refr_now(NULL);
    new_panel = view.callbacks.create_panel(
        view.root, geometry.x, geometry.y,
        geometry.width, geometry.height,
        view.callbacks.context);
    if(new_panel == NULL) {
        set_content_hidden(false);
        return;
    }
    view.panel = new_panel;
    view.geometry = geometry;
    lv_obj_delete(old_panel);
    view.title = NULL;
    view.value = NULL;
    view.scroll_track = NULL;
    view.scroll_thumb = NULL;
    memset(view.rows, 0, sizeof(view.rows));
    memset(view.swatches, 0, sizeof(view.swatches));
    memset(view.labels, 0, sizeof(view.labels));
    memset(view.markers, 0, sizeof(view.markers));

    view.receipt_ring = crazypod_ui_widget_box(
        view.panel, (geometry.width - receipt_size) / 2,
        receipt_y, receipt_size, receipt_size, LV_RADIUS_CIRCLE,
        color, 24);
    lv_obj_set_style_border_width(view.receipt_ring, 2, 0);
    lv_obj_set_style_border_color(
        view.receipt_ring, lv_color_hex(color), 0);
    lv_obj_set_style_border_opa(view.receipt_ring, 220, 0);
    view.receipt_symbol = crazypod_ui_widget_label(
        view.receipt_ring, symbol,
        &lv_font_montserrat_24, color, LV_OPA_COVER);
    lv_obj_center(view.receipt_symbol);
    view.receipt_label = crazypod_ui_widget_label(
        view.panel, resolved_label, view.metadata_font,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(
        view.receipt_label, geometry.width - 20);
    lv_obj_set_height(view.receipt_label, label_height);
    lv_obj_set_style_text_align(
        view.receipt_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(
        view.receipt_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_line_space(
        view.receipt_label, 1, 0);
    lv_obj_set_pos(
        view.receipt_label, 10,
        receipt_y + receipt_size + receipt_gap);
    view.receipt_visible = true;

    lv_anim_delete(view.receipt_ring, NULL);
    if(!animated) {
        lv_obj_set_style_transform_scale(
            view.receipt_ring, LV_SCALE_NONE, 0);
        lv_obj_set_style_opa(
            view.receipt_ring, LV_OPA_COVER, 0);
        return;
    }
    lv_obj_set_style_transform_scale(view.receipt_ring, 176, 0);
    lv_obj_set_style_opa(view.receipt_ring, 0, 0);

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, view.receipt_ring);
    lv_anim_set_exec_cb(&animation, receipt_scale_anim);
    lv_anim_set_values(&animation, 176, LV_SCALE_NONE);
    lv_anim_set_duration(&animation, 240);
    lv_anim_set_path_cb(&animation, lv_anim_path_overshoot);
    lv_anim_start(&animation);

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, view.receipt_ring);
    lv_anim_set_exec_cb(&animation, receipt_opa_anim);
    lv_anim_set_values(&animation, 0, LV_OPA_COVER);
    lv_anim_set_duration(&animation, 150);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);
}

bool crazypod_choice_overlay_visible(void)
{
    return view.kind != 0 && view.root != NULL;
}

bool crazypod_choice_overlay_receipt_visible(void)
{
    return crazypod_choice_overlay_visible() &&
        view.receipt_visible;
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
