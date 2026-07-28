#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "system.h"

#include "lvgl.h"
#include "src/misc/cache/instance/lv_image_cache.h"

#include "../../crazypod_appearance.h"
#include "crazypod_screen_corners.h"

#define SCREEN_COUNT 3
#define RADIUS_MAX 32

static lv_obj_t *masks[SCREEN_COUNT][4];
static uint8_t pixels[4][RADIUS_MAX * RADIUS_MAX * 4]
    CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t descriptors[4];

static void rebuild_descriptors(void)
{
    const struct crazypod_appearance *appearance =
        crazypod_appearance_get();
    int corner;

    for(corner = 0; corner < 4; ++corner) {
        int radius = corner < 2
            ? appearance->screen_top_radius
            : appearance->screen_bottom_radius;
        lv_image_dsc_t *descriptor = &descriptors[corner];
        uint8_t *data = pixels[corner];
        int center_x =
            (corner == 0 || corner == 2) ? radius - 1 : 0;
        int center_y = corner < 2 ? radius - 1 : 0;
        int inner_radius = radius > 0 ? radius - 1 : 0;
        int inner_squared = inner_radius * inner_radius;
        int outer_squared = radius * radius;
        int y;

        if(descriptor->header.magic == LV_IMAGE_HEADER_MAGIC)
            lv_image_cache_drop(descriptor);
        memset(descriptor, 0, sizeof(*descriptor));
        if(radius <= 0)
            continue;
        for(y = 0; y < radius; ++y) {
            int x;

            for(x = 0; x < radius; ++x) {
                int dx = x - center_x;
                int dy = y - center_y;
                int distance_squared = dx * dx + dy * dy;
                unsigned alpha;
                uint8_t *pixel =
                    data + (y * radius + x) * 4;

                if(distance_squared <= inner_squared)
                    alpha = 0;
                else if(distance_squared >= outer_squared)
                    alpha = 255;
                else
                    alpha =
                        (unsigned)(distance_squared - inner_squared) *
                        255 / (outer_squared - inner_squared);
                pixel[0] = 0;
                pixel[1] = 0;
                pixel[2] = 0;
                pixel[3] = alpha;
            }
        }
        descriptor->header.magic = LV_IMAGE_HEADER_MAGIC;
        descriptor->header.cf = LV_COLOR_FORMAT_ARGB8888;
        descriptor->header.w = radius;
        descriptor->header.h = radius;
        descriptor->header.stride = radius * 4;
        descriptor->data_size = radius * radius * 4;
        descriptor->data = data;
    }
}

void crazypod_screen_corners_refresh(void)
{
    const struct crazypod_appearance *appearance =
        crazypod_appearance_get();
    int screen;

    rebuild_descriptors();
    for(screen = 0; screen < SCREEN_COUNT; ++screen) {
        int corner;

        for(corner = 0; corner < 4; ++corner) {
            lv_obj_t *mask = masks[screen][corner];
            int radius = corner < 2
                ? appearance->screen_top_radius
                : appearance->screen_bottom_radius;

            if(mask == NULL)
                continue;
            if(radius <= 0) {
                lv_obj_add_flag(mask, LV_OBJ_FLAG_HIDDEN);
                continue;
            }
            lv_image_set_src(mask, &descriptors[corner]);
            lv_obj_set_size(mask, radius, radius);
            lv_obj_set_pos(
                mask,
                (corner == 0 || corner == 2)
                    ? 0 : LCD_WIDTH - radius,
                corner < 2 ? 0 : LCD_HEIGHT - radius);
            lv_obj_remove_flag(mask, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(mask);
            lv_obj_invalidate(mask);
        }
    }
}

void crazypod_screen_corners_create(
    lv_obj_t *screen, int screen_index)
{
    int corner;

    if(screen_index < 0 || screen_index >= SCREEN_COUNT)
        return;
    for(corner = 0; corner < 4; ++corner) {
        masks[screen_index][corner] = lv_image_create(screen);
        lv_obj_remove_flag(
            masks[screen_index][corner], LV_OBJ_FLAG_CLICKABLE);
    }
    crazypod_screen_corners_refresh();
}

#endif
