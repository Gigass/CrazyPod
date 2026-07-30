#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "kernel.h"
#include "settings.h"
#include "sound.h"

#include "../../../crazypod_lyrics.h"
#include "../../../crazypod_music.h"
#include "../../../crazypod_playlist.h"
#include "../../../crazypod_state.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_now_playing_feature.h"

#define COLOR_DETAIL 0x08080D
#define COLOR_WHITE 0xFFFFFF
#define COLOR_CYAN 0x26CFF5
#define COLOR_FAVORITE 0xFF375F
#define NOW_ACTION_CELL_COUNT 4
#define NOW_PROGRESS_STEP_MS 5000
#define CRAZYPOD_NOW_POPUP_X 35
#define CRAZYPOD_NOW_POPUP_Y 32
#define CRAZYPOD_NOW_POPUP_WIDTH 250
#define CRAZYPOD_NOW_POPUP_HEIGHT 176
#define NOW_VOLUME_HUD_X 0
#define NOW_VOLUME_HUD_WIDTH 18
#define NOW_VOLUME_HUD_HEIGHT 104
#define NOW_VOLUME_TRACK_X 6
#define NOW_VOLUME_TRACK_Y 10
#define NOW_VOLUME_TRACK_WIDTH 6
#define NOW_VOLUME_TRACK_HEIGHT 84
#define NOW_VOLUME_HUD_TICKS \
    ((HZ * 6 / 5) > 0 ? (HZ * 6 / 5) : 1)
#define CRAZYPOD_METADATA_FONT (&lv_font_source_han_sans_sc_14_cjk)

enum now_playing_action {
    NOW_ACTION_QUEUE = 0,
    NOW_ACTION_FAVORITE,
    NOW_ACTION_PLAYBACK,
    NOW_ACTION_LYRICS,
    NOW_ACTION_PROGRESS,
    NOW_ACTION_COUNT,
};

struct now_queue_popup_view {
    lv_obj_t *mode_icon;
    lv_obj_t *mode;
    lv_obj_t *count;
    lv_obj_t *empty;
    lv_obj_t *rows[3];
    lv_obj_t *icons[3];
    lv_obj_t *titles[3];
    lv_obj_t *artists[3];
};

struct now_actions_popup_view {
    lv_obj_t *queue_row;
    lv_obj_t *queue_icon;
    lv_obj_t *queue_label;
    lv_obj_t *cells[NOW_ACTION_CELL_COUNT];
    lv_obj_t *cell_icons[NOW_ACTION_CELL_COUNT];
    lv_obj_t *cell_labels[NOW_ACTION_CELL_COUNT];
    lv_obj_t *detail;
};

struct now_progress_popup_view {
    lv_obj_t *fill;
    lv_obj_t *elapsed;
    lv_obj_t *duration;
    lv_obj_t *icon;
};

struct now_volume_hud_view {
    lv_obj_t *root;
    lv_obj_t *fill;
    lv_obj_t *thumb;
};

static struct crazypod_now_playing_overlay_host overlay_host;
static struct now_queue_popup_view now_queue_view;
static struct now_actions_popup_view now_actions_view;
static struct now_progress_popup_view now_progress_view;
static struct now_volume_hud_view now_volume_view;
static enum crazypod_now_playing_overlay now_overlay;
static int now_action_selected;
static int now_queue_selected;
static unsigned now_queue_generation_seen;
static lv_obj_t *now_overlay_root;
static lv_obj_t *now_overlay_panel;
static bool now_lyrics_mode;
static bool now_favorite_save_failed;
static uint32_t now_progress_elapsed_ms;
static uint32_t now_progress_length_ms;
static long now_progress_follow_after;
static long now_volume_hide_after;
static bool now_volume_fading;

static const struct crazypod_track *current_track(void)
{
    const char *path = crazypod_queue_path(crazypod_queue_index());
    return crazypod_music_track(crazypod_music_find_track(path));
}

static void prepare_now_overlay_glass(bool refresh)
{
    if(overlay_host.prepare_glass != NULL)
        overlay_host.prepare_glass(refresh, overlay_host.context);
}

static void prefetch_now_queue_artwork(int queue_index)
{
    if(overlay_host.prefetch_queue_artwork != NULL)
        overlay_host.prefetch_queue_artwork(
            queue_index, overlay_host.context);
}

static lv_obj_t *make_now_glass_panel(
    int x, int y, int width, int height)
{
    return overlay_host.create_panel != NULL
        ? overlay_host.create_panel(
            now_overlay_root, x, y, width, height,
            overlay_host.context)
        : NULL;
}

static void clear_now_overlay_objects(void)
{
    now_overlay_root = NULL;
    now_overlay_panel = NULL;
    memset(&now_queue_view, 0, sizeof(now_queue_view));
    memset(&now_actions_view, 0, sizeof(now_actions_view));
    memset(&now_progress_view, 0, sizeof(now_progress_view));
}

static void destroy_now_volume_hud(void)
{
    if(now_volume_view.root != NULL &&
       lv_obj_is_valid(now_volume_view.root)) {
        lv_anim_delete(now_volume_view.root, NULL);
        lv_obj_delete(now_volume_view.root);
    }
    memset(&now_volume_view, 0, sizeof(now_volume_view));
    now_volume_hide_after = 0;
    now_volume_fading = false;
}

static void now_popup_y_anim(void *target, int32_t value)
{
    lv_obj_set_y(target, value);
}

static void now_popup_opa_anim(void *target, int32_t value)
{
    lv_obj_set_style_opa(target, (lv_opa_t)value, 0);
}

static void now_volume_x_anim(void *target, int32_t value)
{
    lv_obj_set_x(target, value);
}

static void now_volume_delete_completed(lv_anim_t *animation)
{
    lv_obj_t *root = animation->var;

    if(root != now_volume_view.root || !lv_obj_is_valid(root))
        return;
    lv_obj_delete(root);
    memset(&now_volume_view, 0, sizeof(now_volume_view));
    now_volume_hide_after = 0;
    now_volume_fading = false;
}

static void animate_now_volume_hud_in(void)
{
    lv_anim_t animation;

    lv_obj_set_x(now_volume_view.root, -NOW_VOLUME_HUD_WIDTH);
    lv_obj_set_style_opa(now_volume_view.root, LV_OPA_TRANSP, 0);

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, now_volume_view.root);
    lv_anim_set_exec_cb(&animation, now_volume_x_anim);
    lv_anim_set_values(
        &animation, -NOW_VOLUME_HUD_WIDTH, NOW_VOLUME_HUD_X);
    lv_anim_set_duration(&animation, 130);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, now_volume_view.root);
    lv_anim_set_exec_cb(&animation, now_popup_opa_anim);
    lv_anim_set_values(&animation, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&animation, 100);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);
}

static void create_now_volume_hud(void)
{
    lv_obj_t *track;

    now_volume_view.root = crazypod_ui_widget_box(
        overlay_host.parent,
        NOW_VOLUME_HUD_X,
        (LCD_HEIGHT - NOW_VOLUME_HUD_HEIGHT) / 2,
        NOW_VOLUME_HUD_WIDTH, NOW_VOLUME_HUD_HEIGHT,
        NOW_VOLUME_HUD_WIDTH / 2, 0x050912, 188);
    lv_obj_remove_flag(
        now_volume_view.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width(now_volume_view.root, 1, 0);
    lv_obj_set_style_border_color(
        now_volume_view.root, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(now_volume_view.root, 42, 0);
    lv_obj_set_style_shadow_width(now_volume_view.root, 10, 0);
    lv_obj_set_style_shadow_color(
        now_volume_view.root, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(now_volume_view.root, 96, 0);
    track = crazypod_ui_widget_box(
        now_volume_view.root,
        NOW_VOLUME_TRACK_X, NOW_VOLUME_TRACK_Y,
        NOW_VOLUME_TRACK_WIDTH, NOW_VOLUME_TRACK_HEIGHT,
        LV_RADIUS_CIRCLE, COLOR_WHITE, 46);
    lv_obj_remove_flag(track, LV_OBJ_FLAG_CLICKABLE);
    now_volume_view.fill = crazypod_ui_widget_box(
        now_volume_view.root,
        NOW_VOLUME_TRACK_X, NOW_VOLUME_TRACK_Y,
        NOW_VOLUME_TRACK_WIDTH, NOW_VOLUME_TRACK_HEIGHT,
        LV_RADIUS_CIRCLE, COLOR_CYAN, 232);
    lv_obj_remove_flag(
        now_volume_view.fill, LV_OBJ_FLAG_CLICKABLE);
    now_volume_view.thumb = crazypod_ui_widget_box(
        now_volume_view.root,
        NOW_VOLUME_TRACK_X - 2, NOW_VOLUME_TRACK_Y - 5,
        NOW_VOLUME_TRACK_WIDTH + 4, NOW_VOLUME_TRACK_WIDTH + 4,
        LV_RADIUS_CIRCLE, COLOR_WHITE, LV_OPA_COVER);
    lv_obj_remove_flag(
        now_volume_view.thumb, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_shadow_width(now_volume_view.thumb, 6, 0);
    lv_obj_set_style_shadow_color(
        now_volume_view.thumb, lv_color_hex(COLOR_CYAN), 0);
    lv_obj_set_style_shadow_opa(now_volume_view.thumb, 92, 0);
    animate_now_volume_hud_in();
}

static void refresh_now_volume_hud(int volume)
{
    int minimum = sound_min(SOUND_VOLUME);
    int maximum = sound_max(SOUND_VOLUME);
    int range = maximum - minimum;
    int fill_height;
    int thumb_y;

    if(overlay_host.parent == NULL || range <= 0)
        return;
    if(now_volume_view.root == NULL ||
       !lv_obj_is_valid(now_volume_view.root)) {
        memset(&now_volume_view, 0, sizeof(now_volume_view));
        create_now_volume_hud();
    }
    else if(now_volume_fading) {
        lv_anim_delete(now_volume_view.root, now_popup_opa_anim);
        lv_obj_set_style_opa(
            now_volume_view.root, LV_OPA_COVER, 0);
        now_volume_fading = false;
    }

    fill_height =
        (volume - minimum) * NOW_VOLUME_TRACK_HEIGHT / range;
    if(fill_height < 0)
        fill_height = 0;
    if(fill_height > NOW_VOLUME_TRACK_HEIGHT)
        fill_height = NOW_VOLUME_TRACK_HEIGHT;
    if(fill_height == 0) {
        lv_obj_add_flag(
            now_volume_view.fill, LV_OBJ_FLAG_HIDDEN);
    }
    else {
        lv_obj_remove_flag(
            now_volume_view.fill, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(
            now_volume_view.fill,
            NOW_VOLUME_TRACK_X,
            NOW_VOLUME_TRACK_Y +
                NOW_VOLUME_TRACK_HEIGHT - fill_height);
        lv_obj_set_size(
            now_volume_view.fill,
            NOW_VOLUME_TRACK_WIDTH, fill_height);
    }
    thumb_y = NOW_VOLUME_TRACK_Y +
        NOW_VOLUME_TRACK_HEIGHT - fill_height -
        (NOW_VOLUME_TRACK_WIDTH + 4) / 2;
    lv_obj_set_y(now_volume_view.thumb, thumb_y);
    lv_obj_move_foreground(now_volume_view.root);
    lv_obj_invalidate(now_volume_view.root);
    now_volume_hide_after = current_tick + NOW_VOLUME_HUD_TICKS;
}

static void begin_now_volume_hud_fade(void)
{
    lv_anim_t animation;

    if(now_volume_view.root == NULL ||
       !lv_obj_is_valid(now_volume_view.root)) {
        memset(&now_volume_view, 0, sizeof(now_volume_view));
        return;
    }
    now_volume_fading = true;
    lv_anim_delete(now_volume_view.root, now_popup_opa_anim);
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, now_volume_view.root);
    lv_anim_set_exec_cb(&animation, now_popup_opa_anim);
    lv_anim_set_values(&animation, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_duration(&animation, 140);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in);
    lv_anim_set_completed_cb(
        &animation, now_volume_delete_completed);
    lv_anim_start(&animation);
}

static void animate_now_popup(lv_obj_t *panel, int target_y)
{
    lv_anim_t animation;

    lv_obj_set_y(panel, target_y + 9);
    lv_obj_set_style_opa(panel, 120, 0);

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, panel);
    lv_anim_set_exec_cb(&animation, now_popup_y_anim);
    lv_anim_set_values(&animation, target_y + 9, target_y);
    lv_anim_set_duration(&animation, 180);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, panel);
    lv_anim_set_exec_cb(&animation, now_popup_opa_anim);
    lv_anim_set_values(&animation, 120, LV_OPA_COVER);
    lv_anim_set_duration(&animation, 160);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);
}

static void begin_now_overlay(enum crazypod_now_playing_overlay overlay)
{
    destroy_now_volume_hud();
    if(now_overlay_root != NULL)
        lv_obj_delete(now_overlay_root);
    clear_now_overlay_objects();
    now_overlay = overlay;
    now_overlay_root = crazypod_ui_widget_box(overlay_host.parent, 0, 0,
                                LCD_WIDTH, LCD_HEIGHT, 0,
                                0x000000, 18);
    lv_obj_remove_flag(now_overlay_root, LV_OBJ_FLAG_CLICKABLE);
}

static const char *now_playback_mode_label(void)
{
    if(crazypod_queue_repeat() == REPEAT_ONE)
        return CP_TR("REPEAT 1");
    if(crazypod_queue_repeat() == REPEAT_ALL)
        return CP_TR("REPEAT");
    if(crazypod_queue_shuffle())
        return CP_TR("SHUFFLE");
    return CP_TR("ORDERED");
}

static enum crazypod_ui_icon now_playback_mode_icon(void)
{
    if(crazypod_queue_repeat() == REPEAT_ONE)
        return CRAZYPOD_UI_ICON_REPEAT_ONE;
    if(crazypod_queue_repeat() == REPEAT_ALL)
        return CRAZYPOD_UI_ICON_REPEAT;
    if(crazypod_queue_shuffle())
        return CRAZYPOD_UI_ICON_SHUFFLE;
    return CRAZYPOD_UI_ICON_PLAY;
}

static bool current_track_is_favorite(void)
{
    const struct crazypod_track *track = current_track();

    return track != NULL &&
        crazypod_music_track_is_favorite(track->path);
}

static void refresh_now_favorite_icon(bool selected, bool favorite)
{
    lv_obj_t *icon = now_actions_view.cell_icons[0];

    if(icon == NULL)
        return;
    lv_obj_set_style_opa(
        icon,
        selected ? LV_OPA_COVER : favorite ? 235 : 124, 0);
    crazypod_ui_widget_icon_set_color(
        icon, favorite ? COLOR_FAVORITE : COLOR_WHITE);
}

static void refresh_now_actions_popup(void)
{
    static const int action_for_cell[NOW_ACTION_CELL_COUNT] = {
        NOW_ACTION_FAVORITE, NOW_ACTION_PLAYBACK,
        NOW_ACTION_LYRICS, NOW_ACTION_PROGRESS
    };
    char detail[64];
    int i;
    bool favorite = current_track_is_favorite();
    bool queue_selected = now_action_selected == NOW_ACTION_QUEUE;

    if(now_actions_view.queue_row == NULL)
        return;

    lv_obj_set_style_bg_color(
        now_actions_view.queue_row,
        lv_color_hex(queue_selected ? 0xDBD1BD : COLOR_WHITE), 0);
    lv_obj_set_style_bg_opa(
        now_actions_view.queue_row, queue_selected ? 31 : 7, 0);
    lv_obj_set_style_border_width(now_actions_view.queue_row, 1, 0);
    lv_obj_set_style_border_color(
        now_actions_view.queue_row,
        lv_color_hex(queue_selected ? 0xDBD1BD : COLOR_WHITE), 0);
    lv_obj_set_style_border_opa(
        now_actions_view.queue_row, queue_selected ? 148 : 23, 0);
    lv_obj_set_style_bg_color(
        now_actions_view.queue_icon,
        lv_color_hex(queue_selected ? 0xDBD1BD : COLOR_WHITE), 0);
    lv_obj_set_style_bg_opa(
        now_actions_view.queue_icon, queue_selected ? 224 : 20, 0);
    lv_obj_set_style_text_color(
        now_actions_view.queue_label, lv_color_hex(COLOR_WHITE), 0);
    lv_obj_set_style_text_opa(
        now_actions_view.queue_label, queue_selected ? 250 : 215, 0);

    for(i = 0; i < NOW_ACTION_CELL_COUNT; ++i) {
        bool selected = now_action_selected == action_for_cell[i];

        lv_obj_set_style_bg_color(
            now_actions_view.cells[i],
            lv_color_hex(selected ? 0xDBD1BD : COLOR_WHITE), 0);
        lv_obj_set_style_bg_opa(
            now_actions_view.cells[i], selected ? 38 : 10, 0);
        lv_obj_set_style_border_width(now_actions_view.cells[i], 1, 0);
        lv_obj_set_style_border_color(
            now_actions_view.cells[i],
            lv_color_hex(selected ? 0xDBD1BD : COLOR_WHITE), 0);
        lv_obj_set_style_border_opa(
            now_actions_view.cells[i], selected ? 163 : 31, 0);
        if(i == 0)
            refresh_now_favorite_icon(selected, favorite);
        else
            lv_obj_set_style_opa(
                now_actions_view.cell_icons[i],
                selected ? 245 : 124, 0);
        lv_obj_set_style_text_opa(
            now_actions_view.cell_labels[i], selected ? 230 : 120, 0);
    }

    crazypod_ui_widget_icon_set(
        now_actions_view.cell_icons[1],
        now_playback_mode_icon());
    if(now_action_selected == NOW_ACTION_QUEUE) {
        snprintf(detail, sizeof(detail),
                 CP_FMT("Scroll browse  Center play  Menu exits"));
    }
    else if(now_action_selected == NOW_ACTION_FAVORITE) {
        const struct crazypod_track *track = current_track();

        if(now_favorite_save_failed)
            snprintf(detail, sizeof(detail),
                     CP_FMT("Could not update My Favorites"));
        else if(track == NULL)
            snprintf(detail, sizeof(detail),
                     CP_FMT("No track available"));
        else
            snprintf(detail, sizeof(detail),
                     favorite
                         ? CP_FMT("Saved to My Favorites  Center removes")
                         : CP_FMT("Center adds to My Favorites"));
    }
    else if(now_action_selected == NOW_ACTION_PLAYBACK) {
        snprintf(detail, sizeof(detail), CP_FMT("Center cycles mode  %s"),
                 now_playback_mode_label());
    }
    else if(now_action_selected == NOW_ACTION_LYRICS) {
        const struct crazypod_track *track = current_track();
        bool available = track != NULL &&
                         crazypod_lyrics_load(track->path);
        snprintf(detail, sizeof(detail), "%s  %s",
                 now_lyrics_mode ? CP_FMT("Lyrics visible") : CP_FMT("Lyrics hidden"),
                 available ? CP_FMT("Local LRC") : CP_FMT("No local LRC"));
    }
    else
        snprintf(detail, sizeof(detail),
                 CP_FMT("Center adjusts track progress"));
    CP_LV_LABEL_SET_TEXT(now_actions_view.detail, detail);
}

static void show_now_actions_popup(void)
{
    lv_obj_t *title;
    lv_obj_t *chevron_box;
    lv_obj_t *chevron;
    int i;

    if(now_overlay == CRAZYPOD_NOW_OVERLAY_NONE)
        prepare_now_overlay_glass(true);
    begin_now_overlay(CRAZYPOD_NOW_OVERLAY_ACTIONS);
    if(now_action_selected < 0 ||
       now_action_selected >= NOW_ACTION_COUNT)
        now_action_selected = NOW_ACTION_QUEUE;

    now_overlay_panel = make_now_glass_panel(35, 43, 250, 154);
    title = crazypod_ui_widget_label(now_overlay_panel, CP_TR("ACTIONS"),
                       &lv_font_montserrat_10,
                       COLOR_WHITE, 92);
    lv_obj_set_width(title, 250);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 0, 12);

    now_actions_view.queue_row = crazypod_ui_widget_box(
        now_overlay_panel, 12, 31, 226, 36, 10,
        COLOR_WHITE, LV_OPA_TRANSP);
    now_actions_view.queue_icon = crazypod_ui_widget_box(
        now_actions_view.queue_row, 10, 7, 23, 23,
        LV_RADIUS_CIRCLE, COLOR_WHITE, 20);
    crazypod_ui_widget_icon(
        now_actions_view.queue_icon, 4, 4,
        CRAZYPOD_UI_ICON_LIST, COLOR_DETAIL, 220);
    now_actions_view.queue_label = crazypod_ui_widget_label(
        now_actions_view.queue_row, CP_TR("View Playback Queue"),
        &lv_font_montserrat_10, COLOR_WHITE, 215);
    lv_obj_set_pos(now_actions_view.queue_label, 42, 11);
    chevron_box = crazypod_ui_widget_box(
        now_actions_view.queue_row, 201, 10, 16, 16, 0,
        COLOR_WHITE, LV_OPA_TRANSP);
    chevron = crazypod_ui_widget_label(
        chevron_box, LV_SYMBOL_RIGHT,
        &lv_font_montserrat_8, COLOR_WHITE, 110);
    lv_obj_center(chevron);

    for(i = 0; i < NOW_ACTION_CELL_COUNT; ++i) {
        static const int x_positions[NOW_ACTION_CELL_COUNT] = {
            16, 71, 126, 181
        };
        static const char *const labels[NOW_ACTION_CELL_COUNT] = {
            CP_TR("Favorite"), CP_TR("Playback"), CP_TR("Lyrics"), CP_TR("Progress")
        };
        static const enum crazypod_ui_icon icons[NOW_ACTION_CELL_COUNT] = {
            CRAZYPOD_UI_ICON_HEART,
            CRAZYPOD_UI_ICON_PLAY,
            CRAZYPOD_UI_ICON_FILE,
            CRAZYPOD_UI_ICON_BARS
        };
        int x = x_positions[i];

        now_actions_view.cells[i] = crazypod_ui_widget_box(
            now_overlay_panel, x, 75, 48, 52, 14,
            COLOR_WHITE, 10);
        now_actions_view.cell_icons[i] =
            crazypod_ui_widget_icon(
                now_actions_view.cells[i], 16, 7,
                icons[i], COLOR_WHITE, 124);
        now_actions_view.cell_labels[i] = crazypod_ui_widget_label(
            now_actions_view.cells[i], labels[i],
            &lv_font_montserrat_8,
            COLOR_WHITE, 120);
        lv_obj_set_width(now_actions_view.cell_labels[i], 48);
        lv_obj_set_style_text_align(
            now_actions_view.cell_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(now_actions_view.cell_labels[i], 0, 32);
    }

    now_actions_view.detail = crazypod_ui_widget_label(
        now_overlay_panel, "",
        &lv_font_montserrat_8,
        COLOR_WHITE, 174);
    lv_obj_set_width(now_actions_view.detail, 222);
    lv_obj_set_style_text_align(
        now_actions_view.detail, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(now_actions_view.detail, 14, 134);
    refresh_now_actions_popup();
    animate_now_popup(now_overlay_panel, 43);
}

static void refresh_now_queue_popup(void)
{
    int count = crazypod_queue_count();
    int current = crazypod_queue_index();
    int start;
    char text[32];
    int row;

    if(now_queue_view.count == NULL)
        return;
    if(count <= 0)
        now_queue_selected = 0;
    else {
        if(now_queue_selected < 0)
            now_queue_selected = 0;
        if(now_queue_selected >= count)
            now_queue_selected = count - 1;
    }

    crazypod_ui_widget_icon_set(
        now_queue_view.mode_icon, now_playback_mode_icon());
    CP_LV_LABEL_SET_TEXT(
        now_queue_view.mode, now_playback_mode_label());
    snprintf(text, sizeof(text), count > 0 ? CP_FMT("%d/%d") : "0/0",
             count > 0 ? now_queue_selected + 1 : 0, count);
    CP_LV_LABEL_SET_TEXT(now_queue_view.count, text);
    if(count > 0)
        lv_obj_add_flag(now_queue_view.empty, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_remove_flag(now_queue_view.empty, LV_OBJ_FLAG_HIDDEN);

    start = count <= 3 ? 0 : now_queue_selected - 1;
    if(start < 0)
        start = 0;
    if(start > count - 3)
        start = count - 3;
    for(row = 0; row < 3; ++row) {
        int index = start + row;
        bool selected = index == now_queue_selected;
        bool current_row = index == current;
        const char *path;
        const struct crazypod_track *track;

        if(index < 0 || index >= count) {
            lv_obj_add_flag(now_queue_view.rows[row],
                            LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(now_queue_view.rows[row],
                           LV_OBJ_FLAG_HIDDEN);
        path = crazypod_queue_path(index);
        track = crazypod_music_track(crazypod_music_find_track(path));
        CP_LV_LABEL_SET_TEXT(now_queue_view.titles[row],
                          track != NULL ? track->title : CP_TR("Unavailable"));
        CP_LV_LABEL_SET_TEXT(now_queue_view.artists[row],
                          track != NULL ? track->artist : CP_TR("Local Music"));
        CP_LV_LABEL_SET_TEXT(
            now_queue_view.icons[row],
            current_row
                ? ((audio_status() & AUDIO_STATUS_PAUSE)
                       ? LV_SYMBOL_PAUSE : LV_SYMBOL_VOLUME_MAX)
                : LV_SYMBOL_BULLET);
        lv_obj_set_style_text_color(
            now_queue_view.icons[row],
            lv_color_hex(current_row ? COLOR_CYAN : COLOR_WHITE), 0);
        lv_obj_set_style_text_opa(
            now_queue_view.icons[row], current_row ? 245 : 75, 0);
        lv_obj_set_style_bg_color(
            now_queue_view.rows[row], lv_color_hex(COLOR_WHITE), 0);
        lv_obj_set_style_bg_opa(
            now_queue_view.rows[row], selected ? 41 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_opa(
            now_queue_view.titles[row], selected ? 255 : 220, 0);
        lv_obj_set_style_text_opa(
            now_queue_view.artists[row], selected ? 190 : 140, 0);
    }
    now_queue_generation_seen = crazypod_queue_generation();
}

static void show_now_queue_popup(void)
{
    lv_obj_t *source_title;
    int row;

    if(now_overlay == CRAZYPOD_NOW_OVERLAY_NONE)
        prepare_now_overlay_glass(true);
    begin_now_overlay(CRAZYPOD_NOW_OVERLAY_QUEUE);
    if(crazypod_queue_count() > 0 &&
       (now_queue_selected < 0 ||
        now_queue_selected >= crazypod_queue_count()))
        now_queue_selected = crazypod_queue_index();
    if(crazypod_queue_count() > 0)
        prefetch_now_queue_artwork(now_queue_selected);

    now_overlay_panel = make_now_glass_panel(
        CRAZYPOD_NOW_POPUP_X, CRAZYPOD_NOW_POPUP_Y,
        CRAZYPOD_NOW_POPUP_WIDTH, CRAZYPOD_NOW_POPUP_HEIGHT);
    now_queue_view.mode_icon = crazypod_ui_widget_icon(
        now_overlay_panel, 14, 8,
        now_playback_mode_icon(), COLOR_WHITE, LV_OPA_COVER);
    now_queue_view.mode = crazypod_ui_widget_label(
        now_overlay_panel, "",
        &lv_font_montserrat_10,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(now_queue_view.mode, 36, 12);
    now_queue_view.count = crazypod_ui_widget_label(
        now_overlay_panel, "0/0",
        &lv_font_montserrat_8,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(now_queue_view.count, 54);
    lv_obj_set_style_text_align(
        now_queue_view.count, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(now_queue_view.count, 182, 14);

    crazypod_ui_widget_icon(
        now_overlay_panel, 12, 32,
        CRAZYPOD_UI_ICON_AUDIO, COLOR_WHITE, 225);
    source_title = crazypod_ui_widget_label(now_overlay_panel, CP_TR("Local Queue"),
                              &lv_font_montserrat_8,
                              COLOR_WHITE, 220);
    lv_obj_set_pos(source_title, 34, 36);
    now_queue_view.empty = crazypod_ui_widget_label(
        now_overlay_panel, CP_TR("No Queue"),
        CRAZYPOD_METADATA_FONT,
        COLOR_WHITE, 225);
    lv_obj_set_width(now_queue_view.empty, 222);
    lv_obj_set_style_text_align(
        now_queue_view.empty, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(now_queue_view.empty, 14, 101);

    for(row = 0; row < 3; ++row) {
        int y = 58 + row * 36;
        now_queue_view.rows[row] = crazypod_ui_widget_box(
            now_overlay_panel, 14, y, 222, 32, 6,
            COLOR_WHITE, LV_OPA_TRANSP);
        now_queue_view.icons[row] = crazypod_ui_widget_label(
            now_queue_view.rows[row], LV_SYMBOL_BULLET,
            &lv_font_montserrat_10,
            COLOR_WHITE, 75);
        lv_obj_set_size(now_queue_view.icons[row], 18, 12);
        lv_obj_set_style_text_align(
            now_queue_view.icons[row], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(now_queue_view.icons[row], 7, 10);
        now_queue_view.titles[row] = crazypod_ui_widget_label(
            now_queue_view.rows[row], "",
            CRAZYPOD_METADATA_FONT,
            COLOR_WHITE, 220);
        lv_obj_set_pos(now_queue_view.titles[row], 32, 1);
        lv_obj_set_size(now_queue_view.titles[row], 181, 16);
        lv_label_set_long_mode(
            now_queue_view.titles[row], LV_LABEL_LONG_MODE_DOTS);
        now_queue_view.artists[row] = crazypod_ui_widget_label(
            now_queue_view.rows[row], "",
            CRAZYPOD_METADATA_FONT,
            COLOR_WHITE, 140);
        lv_obj_set_pos(now_queue_view.artists[row], 32, 16);
        lv_obj_set_size(now_queue_view.artists[row], 181, 15);
        lv_label_set_long_mode(
            now_queue_view.artists[row], LV_LABEL_LONG_MODE_DOTS);
    }
    refresh_now_queue_popup();
    animate_now_popup(now_overlay_panel, CRAZYPOD_NOW_POPUP_Y);
}

static void format_now_progress_time(
    uint32_t milliseconds, char *text, size_t capacity)
{
    unsigned seconds = milliseconds / 1000;

    snprintf(text, capacity, "%u:%02u",
             seconds / 60, seconds % 60);
}

static void sync_now_progress_from_playback(void)
{
    const struct mp3entry *id3 = audio_current_track();

    if(id3 == NULL || id3->length <= 0) {
        now_progress_elapsed_ms = 0;
        now_progress_length_ms = 0;
        return;
    }
    now_progress_length_ms = (uint32_t)id3->length;
    now_progress_elapsed_ms =
        id3->elapsed > 0 ? (uint32_t)id3->elapsed : 0;
    if(now_progress_elapsed_ms > now_progress_length_ms)
        now_progress_elapsed_ms = now_progress_length_ms;
}

static void refresh_now_progress_popup(void)
{
    char elapsed[16];
    char duration[16];
    int width = 2;

    if(now_progress_view.fill == NULL)
        return;
    if(now_progress_length_ms > 0 &&
       now_progress_elapsed_ms > 0) {
        width = 2 + (int)(
            (uint64_t)176 * now_progress_elapsed_ms /
            now_progress_length_ms);
    }
    if(width > 178)
        width = 178;
    lv_obj_set_width(now_progress_view.fill, width);
    format_now_progress_time(
        now_progress_elapsed_ms, elapsed, sizeof(elapsed));
    format_now_progress_time(
        now_progress_length_ms, duration, sizeof(duration));
    CP_LV_LABEL_SET_TEXT(now_progress_view.elapsed, elapsed);
    CP_LV_LABEL_SET_TEXT(now_progress_view.duration, duration);
}

static void show_now_progress_popup(void)
{
    lv_obj_t *title;
    lv_obj_t *track;
    lv_obj_t *instruction;

    if(now_overlay == CRAZYPOD_NOW_OVERLAY_NONE)
        prepare_now_overlay_glass(true);
    begin_now_overlay(CRAZYPOD_NOW_OVERLAY_PROGRESS);
    sync_now_progress_from_playback();
    now_progress_follow_after = 0;
    now_overlay_panel = make_now_glass_panel(35, 65, 250, 110);
    title = crazypod_ui_widget_label(now_overlay_panel, CP_TR("PROGRESS"),
                       &lv_font_montserrat_10,
                       COLOR_WHITE, 100);
    lv_obj_set_width(title, 250);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 0, 14);
    now_progress_view.icon = crazypod_ui_widget_icon(
        now_overlay_panel, 22, 47,
        CRAZYPOD_UI_ICON_BARS, COLOR_CYAN, LV_OPA_COVER);
    track = crazypod_ui_widget_box(
        now_overlay_panel, 53, 50, 180, 10,
        LV_RADIUS_CIRCLE, COLOR_WHITE, 48);
    lv_obj_set_style_border_width(track, 1, 0);
    lv_obj_set_style_border_color(
        track, lv_color_hex(COLOR_CYAN), 0);
    lv_obj_set_style_border_opa(track, 190, 0);
    now_progress_view.fill = crazypod_ui_widget_box(
        track, 1, 1, 2, 8, LV_RADIUS_CIRCLE,
        COLOR_CYAN, LV_OPA_COVER);
    now_progress_view.elapsed = crazypod_ui_widget_label(
        now_overlay_panel, "0:00",
        &lv_font_montserrat_10,
        COLOR_WHITE, 235);
    lv_obj_set_width(now_progress_view.elapsed, 85);
    lv_obj_set_pos(now_progress_view.elapsed, 53, 70);
    now_progress_view.duration = crazypod_ui_widget_label(
        now_overlay_panel, "0:00",
        &lv_font_montserrat_10,
        COLOR_WHITE, 235);
    lv_obj_set_width(now_progress_view.duration, 85);
    lv_obj_set_style_text_align(
        now_progress_view.duration, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(now_progress_view.duration, 148, 70);
    instruction = crazypod_ui_widget_label(
        now_overlay_panel, CP_TR("Scroll seeks 5s  Menu exits"),
        &lv_font_montserrat_8,
        COLOR_WHITE, 145);
    lv_obj_set_width(instruction, 250);
    lv_obj_set_style_text_align(
        instruction, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(instruction, 0, 92);
    refresh_now_progress_popup();
    animate_now_popup(now_overlay_panel, 65);
}

static void restore_now_overlay(enum crazypod_now_playing_overlay overlay)
{
    if(overlay == CRAZYPOD_NOW_OVERLAY_ACTIONS)
        show_now_actions_popup();
    else if(overlay == CRAZYPOD_NOW_OVERLAY_QUEUE)
        show_now_queue_popup();
    else if(overlay == CRAZYPOD_NOW_OVERLAY_PROGRESS)
        show_now_progress_popup();
}

static void dismiss_now_overlay(bool refresh_now_playing)
{
    if(now_overlay_root != NULL) {
        lv_anim_delete(now_overlay_panel, NULL);
        lv_obj_delete(now_overlay_root);
    }
    now_overlay = CRAZYPOD_NOW_OVERLAY_NONE;
    clear_now_overlay_objects();
    if(refresh_now_playing && overlay_host.render != NULL)
        overlay_host.render(overlay_host.context);
}


void crazypod_now_playing_overlay_configure(
    const struct crazypod_now_playing_overlay_host *host)
{
    if(host == NULL)
        memset(&overlay_host, 0, sizeof(overlay_host));
    else
        overlay_host = *host;
}

void crazypod_now_playing_overlay_reset(void)
{
    destroy_now_volume_hud();
    now_overlay = CRAZYPOD_NOW_OVERLAY_NONE;
    clear_now_overlay_objects();
}

bool crazypod_now_playing_overlay_visible(void)
{
    return now_overlay != CRAZYPOD_NOW_OVERLAY_NONE;
}

enum crazypod_now_playing_overlay
crazypod_now_playing_overlay_kind(void)
{
    return now_overlay;
}

bool crazypod_now_playing_lyrics_mode(void)
{
    return now_lyrics_mode;
}

void crazypod_now_playing_overlay_show_actions(void)
{
    now_action_selected = NOW_ACTION_QUEUE;
    now_favorite_save_failed = false;
    show_now_actions_popup();
}

void crazypod_now_playing_overlay_show_queue(void)
{
    show_now_queue_popup();
}

void crazypod_now_playing_overlay_show_progress(void)
{
    show_now_progress_popup();
}

void crazypod_now_playing_overlay_restore(
    enum crazypod_now_playing_overlay overlay)
{
    restore_now_overlay(overlay);
}

void crazypod_now_playing_overlay_dismiss(bool refresh)
{
    dismiss_now_overlay(refresh);
}

void crazypod_now_playing_overlay_cycle_playback_mode(void)
{
    if(!crazypod_queue_shuffle() &&
       crazypod_queue_repeat() == REPEAT_OFF)
        crazypod_queue_set_shuffle(true);
    else if(crazypod_queue_shuffle()) {
        crazypod_queue_set_shuffle(false);
        crazypod_queue_set_repeat(REPEAT_ALL);
    }
    else if(crazypod_queue_repeat() == REPEAT_ALL)
        crazypod_queue_set_repeat(REPEAT_ONE);
    else
        crazypod_queue_set_repeat(REPEAT_OFF);

    crazypod_state_mark_dirty();
    if(now_overlay == CRAZYPOD_NOW_OVERLAY_ACTIONS)
        refresh_now_actions_popup();
    else if(now_overlay == CRAZYPOD_NOW_OVERLAY_QUEUE)
        refresh_now_queue_popup();
    else if(overlay_host.render != NULL)
        overlay_host.render(overlay_host.context);
}

void crazypod_now_playing_overlay_activate(void)
{
    if(now_overlay == CRAZYPOD_NOW_OVERLAY_ACTIONS) {
        if(now_action_selected == NOW_ACTION_QUEUE) {
            now_queue_selected = crazypod_queue_count() > 0
                ? crazypod_queue_index() : 0;
            show_now_queue_popup();
        }
        else if(now_action_selected == NOW_ACTION_FAVORITE) {
            const struct crazypod_track *track = current_track();

            now_favorite_save_failed =
                track == NULL ||
                !crazypod_music_toggle_favorite(track->path);
            refresh_now_actions_popup();
        }
        else if(now_action_selected == NOW_ACTION_PLAYBACK)
            crazypod_now_playing_overlay_cycle_playback_mode();
        else if(now_action_selected == NOW_ACTION_LYRICS) {
            now_lyrics_mode = !now_lyrics_mode;
            if(overlay_host.render != NULL)
                overlay_host.render(overlay_host.context);
            show_now_actions_popup();
        }
        else
            show_now_progress_popup();
        return;
    }
    if(now_overlay == CRAZYPOD_NOW_OVERLAY_QUEUE) {
        if(now_queue_selected >= 0 &&
           now_queue_selected < crazypod_queue_count()) {
            playlist_start(now_queue_selected, 0, 0);
            crazypod_state_forget_resume();
            crazypod_state_mark_dirty();
            dismiss_now_overlay(true);
        }
        return;
    }
}

void crazypod_now_playing_adjust_volume(int direction)
{
    int volume = global_status.volume + direction * 2;

    if(direction == 0)
        return;
    if(volume < sound_min(SOUND_VOLUME))
        volume = sound_min(SOUND_VOLUME);
    if(volume > sound_max(SOUND_VOLUME))
        volume = sound_max(SOUND_VOLUME);
    if(volume != global_status.volume) {
        sound_set_volume(volume);
        global_status.volume = volume;
        crazypod_state_mark_dirty();
    }
    refresh_now_volume_hud(volume);
}

static void adjust_now_progress(int direction)
{
    int64_t target;

    if(direction == 0 || now_progress_length_ms == 0)
        return;
    target = (int64_t)now_progress_elapsed_ms +
        (int64_t)direction * NOW_PROGRESS_STEP_MS;
    if(target < 0)
        target = 0;
    if((uint64_t)target >= now_progress_length_ms)
        target = now_progress_length_ms > 0
            ? now_progress_length_ms - 1 : 0;
    if((uint32_t)target == now_progress_elapsed_ms)
        return;

    now_progress_elapsed_ms = (uint32_t)target;
    audio_pre_ff_rewind();
    audio_ff_rewind((long)target);
    crazypod_state_mark_dirty();
    now_progress_follow_after =
        current_tick + (HZ / 2 > 0 ? HZ / 2 : 1);
    refresh_now_progress_popup();
}

void crazypod_now_playing_overlay_move(int direction)
{
    if(now_overlay == CRAZYPOD_NOW_OVERLAY_ACTIONS) {
        int next = now_action_selected + direction;

        if(next < 0)
            next = 0;
        if(next >= NOW_ACTION_COUNT)
            next = NOW_ACTION_COUNT - 1;
        if(next != now_action_selected) {
            now_action_selected = next;
            refresh_now_actions_popup();
        }
    }
    else if(now_overlay == CRAZYPOD_NOW_OVERLAY_QUEUE) {
        int next = now_queue_selected + direction;
        int count = crazypod_queue_count();

        if(count <= 0)
            return;
        if(next < 0)
            next = 0;
        if(next >= count)
            next = count - 1;
        if(next != now_queue_selected) {
            now_queue_selected = next;
            prefetch_now_queue_artwork(now_queue_selected);
            refresh_now_queue_popup();
        }
    }
    else if(now_overlay == CRAZYPOD_NOW_OVERLAY_PROGRESS)
        adjust_now_progress(direction);
}

void crazypod_now_playing_overlay_refresh_queue(void)
{
    if(now_overlay == CRAZYPOD_NOW_OVERLAY_QUEUE)
        refresh_now_queue_popup();
}

void crazypod_now_playing_overlay_refresh_after_playback(void)
{
    crazypod_now_playing_overlay_refresh_queue();
    if(now_overlay == CRAZYPOD_NOW_OVERLAY_PROGRESS &&
       !TIME_BEFORE(current_tick, now_progress_follow_after)) {
        sync_now_progress_from_playback();
        refresh_now_progress_popup();
    }
}

void crazypod_now_playing_overlay_refresh_tick(void)
{
    if(now_volume_view.root != NULL &&
       !now_volume_fading &&
       !TIME_BEFORE(current_tick, now_volume_hide_after))
        begin_now_volume_hud_fade();
    if(now_overlay == CRAZYPOD_NOW_OVERLAY_QUEUE &&
       now_queue_generation_seen != crazypod_queue_generation())
        refresh_now_queue_popup();
    else if(now_overlay == CRAZYPOD_NOW_OVERLAY_PROGRESS &&
            !TIME_BEFORE(current_tick, now_progress_follow_after)) {
        sync_now_progress_from_playback();
        refresh_now_progress_popup();
    }
}

#endif
