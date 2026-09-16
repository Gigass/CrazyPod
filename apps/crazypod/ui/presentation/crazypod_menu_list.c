#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef HAVE_CRAZYPOD_UI

#include <string.h>

#include "crazypod_marquee.h"
#include "crazypod_menu_icon_assets.h"
#include "../../crazypod_state.h"
#include "crazypod_menu_list.h"

#define CRAZYPOD_MENU_LIST_ROWS 6

/*
 * What each row is currently showing.
 *
 * lv_obj_set_style_* invalidates whether or not the value moved, and a
 * wheel step rewrote every property of all six rows even though at most
 * two of them change colour. That is where the thousands of empty-area
 * invalidations in a scrolling perf log came from, and why a step that
 * only moves the highlight repainted the whole list column.
 */
struct crazypod_menu_row_state {
    bool set;
    bool hidden;
    bool selected;
    bool gradient;
    uint32_t color;
    uint32_t grad_color;
    lv_opa_t opa;
    lv_opa_t label_opa;
    lv_opa_t circle_opa;
    lv_opa_t icon_opa;
    lv_opa_t marker_opa;
};

struct crazypod_menu_list_view {
    bool valid;
    enum crazypod_route route;
    lv_obj_t *rows[CRAZYPOD_MENU_LIST_ROWS];
    lv_obj_t *labels[CRAZYPOD_MENU_LIST_ROWS];
    lv_obj_t *markers[CRAZYPOD_MENU_LIST_ROWS];
    lv_obj_t *circles[CRAZYPOD_MENU_LIST_ROWS];
    lv_obj_t *icons[CRAZYPOD_MENU_LIST_ROWS];
    const void *icon_src[CRAZYPOD_MENU_LIST_ROWS];
    struct crazypod_menu_row_state state[CRAZYPOD_MENU_LIST_ROWS];
    lv_obj_t *scroll_thumb;
};

static struct crazypod_menu_list_view view;

void crazypod_menu_list_reset(enum crazypod_route route)
{
    memset(&view, 0, sizeof(view));
    view.valid = true;
    view.route = route;
}

void crazypod_menu_list_clear(void)
{
    memset(&view, 0, sizeof(view));
}

bool crazypod_menu_list_matches(enum crazypod_route route)
{
    return view.valid && view.route == route;
}

void crazypod_menu_list_bind_row(int row, lv_obj_t *box,
                                 lv_obj_t *label, lv_obj_t *marker)
{
    if(row < 0 || row >= CRAZYPOD_MENU_LIST_ROWS)
        return;
    view.rows[row] = box;
    view.labels[row] = label;
    view.markers[row] = marker;
    memset(&view.state[row], 0, sizeof(view.state[row]));
}

void crazypod_menu_list_bind_icon(int row, lv_obj_t *circle,
                                  lv_obj_t *icon)
{
    if(row < 0 || row >= CRAZYPOD_MENU_LIST_ROWS)
        return;
    view.circles[row] = circle;
    view.icons[row] = icon;
    view.icon_src[row] = NULL;
}

void crazypod_menu_list_bind_scroll_thumb(lv_obj_t *thumb)
{
    view.scroll_thumb = thumb;
}

void crazypod_menu_list_refresh_row(
    int row, bool visible, const char *title, bool selected,
    lv_opa_t label_opa, uint32_t panel_color, uint32_t primary_color,
    uint32_t secondary_color, bool gradient,
    enum crazypod_menu_icon icon,
    lv_opa_t icon_opa, const char *marker_text, lv_opa_t marker_opa)
{
    struct crazypod_menu_row_state next;
    struct crazypod_menu_row_state *shown;
    lv_obj_t *box;
    bool force;

    if(row < 0 || row >= CRAZYPOD_MENU_LIST_ROWS)
        return;
    box = view.rows[row];
    if(box == NULL)
        return;
    shown = &view.state[row];

    if(!visible) {
        if(shown->set && shown->hidden)
            return;
        crazypod_marquee_configure(view.labels[row], false);
        lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);
        memset(shown, 0, sizeof(*shown));
        shown->set = true;
        shown->hidden = true;
        return;
    }

    /* Everything the row draws, resolved before anything is written, so
     * an unchanged property costs a comparison rather than a redraw. */
    memset(&next, 0, sizeof(next));
    next.set = true;
    next.hidden = false;
    next.selected = selected;
    next.gradient = selected && gradient &&
        !crazypod_state_reduce_effects();
    next.color = selected ? primary_color : panel_color;
    next.grad_color = secondary_color;
    next.opa = selected ? 220 : LV_OPA_TRANSP;
    next.label_opa = label_opa;
    /* Under Reduce Effects the faint unselected circle is dropped: it is
     * one anti-aliased rounded fill per row for 7% opacity. */
    next.circle_opa = selected ? 45
        : crazypod_state_reduce_effects() ? LV_OPA_TRANSP : 18;
    next.icon_opa = icon_opa;
    next.marker_opa = marker_opa;

    /*
     * A hidden row keeps whatever it was last painted with, so its cached
     * state says nothing about the object. Write everything when one
     * comes back, or an unselected row that was hidden while selected
     * stays painted as selected: its target opacity is zero, and so is
     * the cleared cache it would be compared against.
     */
    force = !shown->set || shown->hidden;
    if(force)
        lv_obj_remove_flag(box, LV_OBJ_FLAG_HIDDEN);

    if(view.labels[row] != NULL) {
        crazypod_marquee_set_text(
            view.labels[row], title != NULL ? title : "", selected);
        if(force || shown->label_opa != next.label_opa)
            lv_obj_set_style_text_opa(
                view.labels[row], next.label_opa, 0);
    }
    if(force || shown->color != next.color)
        lv_obj_set_style_bg_color(box, lv_color_hex(next.color), 0);
    if(force || shown->opa != next.opa)
        lv_obj_set_style_bg_opa(box, next.opa, 0);
    if(force || shown->selected != next.selected)
        lv_obj_set_style_border_width(box, selected ? 1 : 0, 0);
    if(next.gradient) {
        if(force || !shown->gradient ||
           shown->grad_color != next.grad_color)
            lv_obj_set_style_bg_grad_color(
                box, lv_color_hex(next.grad_color), 0);
        if(force || !shown->gradient)
            lv_obj_set_style_bg_grad_dir(box, LV_GRAD_DIR_HOR, 0);
    }
    else if(force || shown->gradient)
        lv_obj_set_style_bg_grad_dir(box, LV_GRAD_DIR_NONE, 0);

    if(view.circles[row] != NULL) {
        if(force || shown->circle_opa != next.circle_opa)
            lv_obj_set_style_bg_opa(
                view.circles[row], next.circle_opa, 0);
        if(view.icons[row] != NULL) {
            /* lv_image_set_src invalidates unconditionally, and these
             * assets are static, so a row that keeps its icon across a
             * wheel step should not pay for a redraw of it. */
            const void *asset = crazypod_menu_icon_asset(icon);

            if(view.icon_src[row] != asset) {
                view.icon_src[row] = asset;
                lv_image_set_src(view.icons[row], asset);
            }
            if(force || shown->icon_opa != next.icon_opa)
                lv_obj_set_style_opa(
                    view.icons[row], next.icon_opa, 0);
        }
    }
    if(view.markers[row] != NULL) {
        CP_LV_LABEL_SET_TEXT(
            view.markers[row], marker_text != NULL ? marker_text : "");
        if(force || shown->marker_opa != next.marker_opa)
            lv_obj_set_style_text_opa(
                view.markers[row], next.marker_opa, 0);
    }
    *shown = next;
}

void crazypod_menu_list_refresh_scroll(int y, int height)
{
    if(view.scroll_thumb == NULL)
        return;
    lv_obj_set_y(view.scroll_thumb, y);
    lv_obj_set_height(view.scroll_thumb, height);
}

#endif
