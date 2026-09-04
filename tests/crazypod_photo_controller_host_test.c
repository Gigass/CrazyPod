#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "crazypod_photo_controller.h"

static bool favorite;

const lv_image_dsc_t *crazypod_photo_render_viewport(
    int index, int zoom_percent, int *pan_x, int *pan_y)
{
    (void)index;
    (void)zoom_percent;
    (void)pan_x;
    (void)pan_y;
    return NULL;
}

bool crazypod_photo_is_favorite(int index)
{
    (void)index;
    return favorite;
}

bool crazypod_photo_toggle_favorite(int index)
{
    (void)index;
    favorite = !favorite;
    return true;
}

int main(void)
{
    bool activate;
    bool remove_progress;
    const struct crazypod_photo_controller_model *model;

    crazypod_photo_controller_reset();
    crazypod_photo_controller_begin_select(true, 100);
    crazypod_photo_controller_cancel_select();
    crazypod_photo_controller_release_select(
        &activate, &remove_progress);
    assert(!activate);
    assert(!remove_progress);

    crazypod_photo_controller_open_detail(200);
    crazypod_photo_controller_wheel_sample(0, 200, 20, 24, 8);
    crazypod_photo_controller_wheel_sample(10, 201, 20, 24, 8);
    crazypod_photo_controller_cancel_wheel();
    crazypod_photo_controller_wheel_sample(-1, 202, 20, 24, 8);
    model = crazypod_photo_controller_model();
    assert(model->pan_x == 0);
    assert(model->pan_y == 0);
    assert(!crazypod_photo_controller_take_pan_render());

    return 0;
}
