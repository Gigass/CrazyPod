#include "config.h"

#ifdef IPOD_6G

#include <string.h>

#include "crazypod_marquee.h"

#define CRAZYPOD_MARQUEE_SPEED 30
#define CRAZYPOD_MARQUEE_START_DELAY_MS 800

static lv_anim_t marquee_template;
static bool marquee_template_ready;

static void prepare_template(void)
{
    if(marquee_template_ready)
        return;
    lv_anim_init(&marquee_template);
    lv_anim_set_delay(
        &marquee_template,
        CRAZYPOD_MARQUEE_START_DELAY_MS);
    lv_anim_set_repeat_count(
        &marquee_template, LV_ANIM_REPEAT_INFINITE);
    marquee_template_ready = true;
}

void crazypod_marquee_configure(lv_obj_t *label, bool active)
{
    lv_label_long_mode_t mode = active
        ? LV_LABEL_LONG_MODE_SCROLL_CIRCULAR
        : LV_LABEL_LONG_MODE_DOTS;
    uint32_t speed = lv_anim_speed(CRAZYPOD_MARQUEE_SPEED);

    if(label == NULL)
        return;
    prepare_template();
    if(lv_obj_get_style_anim_duration(
           label, LV_PART_MAIN) != speed)
        lv_obj_set_style_anim_duration(label, speed, 0);
    if(lv_obj_get_style_anim(
           label, LV_PART_MAIN) != &marquee_template)
        lv_obj_set_style_anim(
            label, &marquee_template, 0);
    if(lv_label_get_long_mode(label) != mode)
        lv_label_set_long_mode(label, mode);
}

void crazypod_marquee_set_text(
    lv_obj_t *label, const char *text, bool active)
{
    bool changed;

    if(label == NULL)
        return;
    if(text == NULL)
        text = "";
    changed = strcmp(lv_label_get_text(label), text) != 0;
    if(changed)
        lv_label_set_text(label, text);
    crazypod_marquee_configure(label, active);
    if(changed)
        lv_label_set_long_mode(
            label, active
                ? LV_LABEL_LONG_MODE_SCROLL_CIRCULAR
                : LV_LABEL_LONG_MODE_DOTS);
}

#endif
