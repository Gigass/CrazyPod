#include "config.h"

#include "../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <string.h>

#include "crazypod_marquee.h"

#define CRAZYPOD_MARQUEE_SPEED 30
#define CRAZYPOD_MARQUEE_PAUSE_MS 2000

static lv_anim_t marquee_template;
static bool marquee_template_ready;

static lv_label_long_mode_t marquee_mode(
    lv_obj_t *label, bool active)
{
    const lv_font_t *font;
    const char *text;
    lv_point_t text_size;
    int32_t content_width;

    if(!active)
        return LV_LABEL_LONG_MODE_DOTS;
    text = lv_label_get_text(label);
    font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    content_width =
        lv_obj_calc_dynamic_width(label, LV_STYLE_WIDTH) -
        lv_obj_get_style_space_left(label, LV_PART_MAIN) -
        lv_obj_get_style_space_right(label, LV_PART_MAIN);
    if(text == NULL || text[0] == '\0' ||
       font == NULL || content_width <= 0)
        return LV_LABEL_LONG_MODE_DOTS;
    lv_text_get_size(
        &text_size, text, font,
        lv_obj_get_style_text_letter_space(label, LV_PART_MAIN),
        lv_obj_get_style_text_line_space(label, LV_PART_MAIN),
        LV_COORD_MAX, LV_TEXT_FLAG_EXPAND);
    return text_size.x > content_width
        ? LV_LABEL_LONG_MODE_SCROLL_CIRCULAR
        : LV_LABEL_LONG_MODE_DOTS;
}

static void prepare_template(void)
{
    if(marquee_template_ready)
        return;
    lv_anim_init(&marquee_template);
    lv_anim_set_delay(
        &marquee_template,
        CRAZYPOD_MARQUEE_PAUSE_MS);
    lv_anim_set_repeat_count(
        &marquee_template, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_repeat_delay(
        &marquee_template,
        CRAZYPOD_MARQUEE_PAUSE_MS);
    marquee_template_ready = true;
}

static void configure(
    lv_obj_t *label, bool active, bool center_when_static)
{
    lv_label_long_mode_t mode;
    uint32_t speed = lv_anim_speed(CRAZYPOD_MARQUEE_SPEED);

    if(label == NULL)
        return;
    mode = marquee_mode(label, active);
    prepare_template();
    if(center_when_static)
        lv_obj_set_style_text_align(
            label,
            mode == LV_LABEL_LONG_MODE_SCROLL_CIRCULAR
                ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_CENTER,
            0);
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

void crazypod_marquee_configure(lv_obj_t *label, bool active)
{
    configure(label, active, false);
}

void crazypod_marquee_configure_centered(
    lv_obj_t *label, bool active)
{
    configure(label, active, true);
}

void crazypod_marquee_set_paused(lv_obj_t *label, bool paused)
{
    lv_anim_t *animation;

    if(label == NULL)
        return;
    animation = lv_anim_get(label, NULL);
    if(animation == NULL || lv_anim_is_paused(animation) == paused)
        return;
    if(paused)
        lv_anim_pause(animation);
    else
        lv_anim_resume(animation);
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
        CP_LV_LABEL_SET_TEXT(label, text);
    crazypod_marquee_configure(label, active);
    if(changed)
        lv_label_set_long_mode(label, marquee_mode(label, active));
}

#endif
