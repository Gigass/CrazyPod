#include "config.h"

#ifdef IPOD_6G

#include <string.h>


#include "lcd.h"
#include "system.h"

#include "src/misc/cache/instance/lv_image_cache.h"

#include "../../crazypod_frameclock.h"
#include "../../crazypod_image.h"
#include "../../platform/crazypod_platform_display.h"
#include "../presentation/crazypod_scene_motion.h"
#include "../presentation/crazypod_ui_widgets.h"
#include "crazypod_scene_transition.h"

#define FRAME_PIXELS (LCD_WIDTH * LCD_HEIGHT)
#define EDGE_SHADOW_WIDTH 28
#define EDGE_SHADOW_OFFSET_Y (-10)

static fb_data from_pixels[FRAME_PIXELS]
    CACHEALIGN_AT_LEAST_ATTR(16);
static fb_data to_pixels[FRAME_PIXELS]
    CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t from_descriptor;
static lv_image_dsc_t to_descriptor;

static struct {
    enum crazypod_scene_motion_kind kind;
    lv_obj_t *overlay;
    lv_obj_t *from_page;
    lv_obj_t *to_page;
    bool prepared;
    bool active;
    bool finishing;
    uint32_t finish_sequence;
} transition;


static void drop_snapshot_cache(void)
{
    if(from_descriptor.header.magic == LV_IMAGE_HEADER_MAGIC)
        lv_image_cache_drop(&from_descriptor);
    if(to_descriptor.header.magic == LV_IMAGE_HEADER_MAGIC)
        lv_image_cache_drop(&to_descriptor);
}

static void destroy_overlay(void)
{
    lv_obj_t *overlay = transition.overlay;

    transition.overlay = NULL;
    transition.from_page = NULL;
    transition.to_page = NULL;
    transition.active = false;
    transition.prepared = false;
    transition.finishing = false;
    transition.finish_sequence = 0;
    if(overlay != NULL && lv_obj_is_valid(overlay)) {
        lv_anim_delete(overlay, NULL);
        lv_obj_delete(overlay);
    }
    drop_snapshot_cache();
}

static lv_obj_t *create_page(
    lv_obj_t *parent, const lv_image_dsc_t *descriptor)
{
    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_t *image;

    crazypod_ui_widget_make_plain(page);
    lv_obj_set_pos(page, 0, 0);
    lv_obj_set_size(page, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_style_bg_color(page, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_remove_flag(page, LV_OBJ_FLAG_CLICKABLE);

    image = lv_image_create(page);
    lv_image_set_src(image, descriptor);
    lv_obj_set_pos(image, 0, 0);
    lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
    return page;
}

static void configure_edge_shadow(lv_obj_t *page)
{
    lv_obj_set_style_shadow_width(page, EDGE_SHADOW_WIDTH, 0);
    lv_obj_set_style_shadow_offset_x(page, 0, 0);
    lv_obj_set_style_shadow_offset_y(page, EDGE_SHADOW_OFFSET_Y, 0);
    lv_obj_set_style_shadow_spread(page, 0, 0);
    lv_obj_set_style_shadow_color(page, lv_color_hex(0x000000), 0);
}

static void apply_layout(int progress)
{
    struct crazypod_scene_motion_layout layout;
    lv_obj_t *shadow_page;

    if(!transition.active || transition.overlay == NULL)
        return;
    crazypod_scene_motion_layout(
        transition.kind, progress, LCD_HEIGHT, &layout);
    lv_obj_set_pos(
        transition.from_page, layout.from_x, layout.from_y);
    lv_obj_set_pos(transition.to_page, layout.to_x, layout.to_y);
    shadow_page = transition.kind == CRAZYPOD_SCENE_MOTION_POP
        ? transition.from_page : transition.to_page;
    lv_obj_set_style_shadow_opa(
        shadow_page, layout.edge_shadow_opacity, 0);
}

static void transition_anim(void *context, int32_t value)
{
    if(context == transition.overlay)
        apply_layout((int)value);
}

static void transition_completed(lv_anim_t *animation)
{
    if(animation->var != transition.overlay)
        return;
    transition.finishing = true;
    transition.finish_sequence =
        crazypod_present_sequence() + 1;
    crazypod_present_queue_full();
}

bool crazypod_scene_transition_begin(
    enum crazypod_scene_motion_kind kind)
{
    const fb_data *framebuffer =
        crazypod_platform_display_framebuffer();

    if(kind == CRAZYPOD_SCENE_MOTION_NONE || framebuffer == NULL)
        return false;
    if(transition.active || transition.prepared)
        crazypod_scene_transition_finish();
    crazypod_present_take_fullscreen_ownership();
    memcpy(from_pixels, framebuffer, sizeof(from_pixels));
    transition.kind = kind;
    transition.prepared = true;
    return true;
}

bool crazypod_scene_transition_commit(lv_obj_t *parent)
{
    const fb_data *framebuffer =
        crazypod_platform_display_framebuffer();
    lv_anim_t animation;
    lv_obj_t *first_page;
    lv_obj_t *second_page;

    if(!transition.prepared || parent == NULL ||
       !lv_obj_is_valid(parent) || framebuffer == NULL)
        return false;

    lv_refr_now(NULL);
    memcpy(to_pixels, framebuffer, sizeof(to_pixels));
    drop_snapshot_cache();
    if(!crazypod_image_configure_rgb565(
           &from_descriptor, from_pixels, LCD_WIDTH, LCD_HEIGHT) ||
       !crazypod_image_configure_rgb565(
           &to_descriptor, to_pixels, LCD_WIDTH, LCD_HEIGHT)) {
        transition.prepared = false;
        return false;
    }

    transition.overlay = lv_obj_create(parent);
    crazypod_ui_widget_make_plain(transition.overlay);
    lv_obj_set_pos(transition.overlay, 0, 0);
    lv_obj_set_size(transition.overlay, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_style_bg_opa(
        transition.overlay, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(transition.overlay, LV_OBJ_FLAG_CLICKABLE);

    if(transition.kind == CRAZYPOD_SCENE_MOTION_POP) {
        first_page = create_page(
            transition.overlay, &to_descriptor);
        second_page = create_page(
            transition.overlay, &from_descriptor);
        transition.to_page = first_page;
        transition.from_page = second_page;
        configure_edge_shadow(transition.from_page);
    }
    else {
        first_page = create_page(
            transition.overlay, &from_descriptor);
        second_page = create_page(
            transition.overlay, &to_descriptor);
        transition.from_page = first_page;
        transition.to_page = second_page;
        if(transition.kind == CRAZYPOD_SCENE_MOTION_PUSH ||
           transition.kind == CRAZYPOD_SCENE_MOTION_REPLACE)
            configure_edge_shadow(transition.to_page);
    }

    transition.active = true;
    transition.prepared = false;
    lv_obj_move_foreground(transition.overlay);
    apply_layout(0);
    lv_refr_now(NULL);
    crazypod_present_queue_full();

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, transition.overlay);
    lv_anim_set_exec_cb(&animation, transition_anim);
    lv_anim_set_values(
        &animation, 0, CRAZYPOD_SCENE_MOTION_PROGRESS_MAX);
    lv_anim_set_duration(
        &animation,
        crazypod_scene_motion_duration_ms(transition.kind));
    lv_anim_set_path_cb(&animation, lv_anim_path_linear);
    lv_anim_set_completed_cb(&animation, transition_completed);
    lv_anim_set_early_apply(&animation, true);
    lv_anim_start(&animation);
    return true;
}

bool crazypod_scene_transition_active(void)
{
    return transition.active;
}

bool crazypod_scene_transition_owns_framebuffer(void)
{
    return transition.prepared || transition.active;
}

void crazypod_scene_transition_service(void)
{
    if(!transition.finishing ||
       (int32_t)(crazypod_present_sequence() -
                 transition.finish_sequence) < 0)
        return;
    destroy_overlay();
}

void crazypod_scene_transition_finish(void)
{
    bool redraw = transition.active || transition.prepared;

    if(transition.active)
        apply_layout(CRAZYPOD_SCENE_MOTION_PROGRESS_MAX);
    destroy_overlay();
    if(redraw) {
        lv_refr_now(NULL);
        crazypod_present_queue_full();
    }
}

void crazypod_scene_transition_reset(void)
{
    destroy_overlay();
    memset(&transition, 0, sizeof(transition));
}

#endif
