#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <string.h>

#include "crazypod_marquee.h"
#include "crazypod_menu_icon_assets.h"
#include "crazypod_menu_list.h"

#define CRAZYPOD_MENU_LIST_ROWS 6

struct crazypod_menu_list_view {
    bool valid;
    enum crazypod_route route;
    lv_obj_t *rows[CRAZYPOD_MENU_LIST_ROWS];
    lv_obj_t *labels[CRAZYPOD_MENU_LIST_ROWS];
    lv_obj_t *markers[CRAZYPOD_MENU_LIST_ROWS];
    lv_obj_t *circles[CRAZYPOD_MENU_LIST_ROWS];
    lv_obj_t *icons[CRAZYPOD_MENU_LIST_ROWS];
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
}

void crazypod_menu_list_bind_icon(int row, lv_obj_t *circle,
                                  lv_obj_t *icon)
{
    if(row < 0 || row >= CRAZYPOD_MENU_LIST_ROWS)
        return;
    view.circles[row] = circle;
    view.icons[row] = icon;
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
    lv_obj_t *box;

    if(row < 0 || row >= CRAZYPOD_MENU_LIST_ROWS)
        return;
    box = view.rows[row];
    if(box == NULL)
        return;
    if(!visible) {
        crazypod_marquee_configure(
            view.labels[row], false);
        lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_remove_flag(box, LV_OBJ_FLAG_HIDDEN);
    if(view.labels[row] != NULL) {
        crazypod_marquee_set_text(
            view.labels[row],
            title != NULL ? title : "",
            selected);
        lv_obj_set_style_text_opa(view.labels[row], label_opa, 0);
    }
    lv_obj_set_style_bg_color(
        box, lv_color_hex(selected ? primary_color : panel_color), 0);
    lv_obj_set_style_bg_opa(
        box, selected ? 220 : LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(box, selected ? 1 : 0, 0);
    if(selected && gradient) {
        lv_obj_set_style_bg_grad_color(
            box, lv_color_hex(secondary_color), 0);
        lv_obj_set_style_bg_grad_dir(box, LV_GRAD_DIR_HOR, 0);
    }

    if(view.circles[row] != NULL) {
        lv_obj_set_style_bg_opa(
            view.circles[row], selected ? 45 : 18, 0);
        if(view.icons[row] != NULL) {
            lv_image_set_src(
                view.icons[row], crazypod_menu_icon_asset(icon));
            lv_obj_set_style_opa(view.icons[row], icon_opa, 0);
        }
    }
    if(view.markers[row] != NULL) {
        CP_LV_LABEL_SET_TEXT(
            view.markers[row], marker_text != NULL ? marker_text : "");
        lv_obj_set_style_text_opa(view.markers[row], marker_opa, 0);
    }
}

void crazypod_menu_list_refresh_scroll(int y, int height)
{
    if(view.scroll_thumb == NULL)
        return;
    lv_obj_set_y(view.scroll_thumb, y);
    lv_obj_set_height(view.scroll_thumb, height);
}

#endif
