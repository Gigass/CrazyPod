#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "iap-usb.h"
#include "settings.h"
#include "sound.h"

#include "../../../crazypod_lyrics.h"
#include "../../../crazypod_music.h"
#include "../../../crazypod_playlist.h"
#include "../../../crazypod_runtime_font.h"
#include "../../../crazypod_state.h"
#include "../../presentation/crazypod_marquee.h"
#include "../../presentation/crazypod_popup_layout.h"
#include "../../presentation/crazypod_popup_motion.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_now_playing_feature.h"

#define COLOR_DETAIL 0x08080D
#define COLOR_WHITE 0xFFFFFF
#define COLOR_CYAN 0x26CFF5
#define COLOR_FAVORITE 0xFF375F
#define NOW_ACTION_CELL_COUNT 4
#define NOW_PLAYBACK_MODE_COUNT 4
#define NOW_PLAYBACK_ROW_HEIGHT 31
#define NOW_QUEUE_HEADER_HEIGHT 34
#define NOW_QUEUE_ROW_HEIGHT 28
#define NOW_QUEUE_POPUP_WIDTH 200
#define NOW_PROGRESS_STEP_MS 5000
#define NOW_VOLUME_HUD_X 0
#define NOW_VOLUME_HUD_WIDTH 18
#define NOW_VOLUME_HUD_HEIGHT 104
#define NOW_VOLUME_TRACK_X 6
#define NOW_VOLUME_TRACK_Y 10
#define NOW_VOLUME_TRACK_WIDTH 6
#define NOW_VOLUME_TRACK_HEIGHT 84
#define NOW_VOLUME_HUD_MS 1200
#define CRAZYPOD_METADATA_FONT (crazypod_runtime_font_at_size(18))
#define NOW_QUEUE_TITLE_FONT (crazypod_runtime_font_at_size(15))

enum now_playing_action {
    NOW_ACTION_QUEUE = 0,
    NOW_ACTION_FAVORITE,
    NOW_ACTION_PLAYBACK,
    NOW_ACTION_LYRICS,
    NOW_ACTION_PROGRESS,
    NOW_ACTION_COUNT,
};

enum now_playback_mode {
    NOW_PLAYBACK_ORDERED = 0,
    NOW_PLAYBACK_SHUFFLE,
    NOW_PLAYBACK_REPEAT_ALL,
    NOW_PLAYBACK_REPEAT_ONE,
};

struct now_queue_popup_view {
    lv_obj_t *mode_icon;
    lv_obj_t *mode;
    lv_obj_t *count;
    lv_obj_t *empty;
    lv_obj_t *rows[3];
    lv_obj_t *icons[3];
    lv_obj_t *titles[3];
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

struct now_playback_popup_view {
    lv_obj_t *rows[NOW_PLAYBACK_MODE_COUNT];
    lv_obj_t *icons[NOW_PLAYBACK_MODE_COUNT];
    lv_obj_t *labels[NOW_PLAYBACK_MODE_COUNT];
    lv_obj_t *markers[NOW_PLAYBACK_MODE_COUNT];
};

struct now_progress_popup_view {
    lv_obj_t *fill;
    lv_obj_t *elapsed;
    lv_obj_t *duration;
    lv_obj_t *icon;
    int fill_max_width;
};

struct now_volume_hud_view {
    lv_obj_t *root;
    lv_obj_t *fill;
    lv_obj_t *thumb;
    lv_timer_t *hide_timer;
};

static struct crazypod_now_playing_overlay_host overlay_host;
static struct now_queue_popup_view now_queue_view;
static struct now_actions_popup_view now_actions_view;
static struct now_playback_popup_view now_playback_view;
static struct now_progress_popup_view now_progress_view;
static struct now_volume_hud_view now_volume_view;
static enum crazypod_now_playing_overlay now_overlay;
static int now_action_selected;
static int now_playback_selected;
static int now_queue_selected;
static unsigned now_queue_generation_seen;
static lv_obj_t *now_overlay_root;
static lv_obj_t *now_overlay_panel;
static uint32_t now_progress_elapsed_ms;
static uint32_t now_progress_length_ms;
static bool now_progress_dirty;
static char now_progress_track_path[MAX_PATH];
static bool now_volume_fading;

static bool copy_current_track(struct crazypod_track *track)
{
    char path[MAX_PATH];

    return crazypod_queue_copy_path(
            crazypod_queue_index(), path, sizeof(path)) &&
        crazypod_music_copy_track(crazypod_music_find_track(path), track);
}

static void prepare_now_overlay_glass(bool refresh)
{
    if(overlay_host.prepare_glass != NULL)
        overlay_host.prepare_glass(refresh, overlay_host.context);
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
    memset(&now_playback_view, 0, sizeof(now_playback_view));
    memset(&now_progress_view, 0, sizeof(now_progress_view));
}

static void destroy_now_overlay_objects(void)
{
    if(now_overlay_panel != NULL &&
       lv_obj_is_valid(now_overlay_panel))
        lv_anim_delete(now_overlay_panel, NULL);
    if(now_overlay_root != NULL &&
       lv_obj_is_valid(now_overlay_root))
        lv_obj_delete(now_overlay_root);
    clear_now_overlay_objects();
}

static void destroy_now_volume_hud(void)
{
    if(now_volume_view.hide_timer != NULL) {
        lv_timer_delete(now_volume_view.hide_timer);
        now_volume_view.hide_timer = NULL;
    }
    if(now_volume_view.root != NULL &&
       lv_obj_is_valid(now_volume_view.root)) {
        lv_anim_delete(now_volume_view.root, NULL);
        lv_obj_delete(now_volume_view.root);
    }
    memset(&now_volume_view, 0, sizeof(now_volume_view));
    now_volume_fading = false;
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
    now_volume_fading = false;
}

static void begin_now_volume_hud_fade(void);

static void now_volume_hide_timer(lv_timer_t *timer)
{
    if(timer != now_volume_view.hide_timer)
        return;
    now_volume_view.hide_timer = NULL;
    lv_timer_delete(timer);
    begin_now_volume_hud_fade();
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
        destroy_now_volume_hud();
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
    if(now_volume_view.hide_timer == NULL)
        now_volume_view.hide_timer = lv_timer_create(
            now_volume_hide_timer, NOW_VOLUME_HUD_MS, NULL);
    else {
        lv_timer_set_period(
            now_volume_view.hide_timer, NOW_VOLUME_HUD_MS);
        lv_timer_reset(now_volume_view.hide_timer);
        lv_timer_resume(now_volume_view.hide_timer);
    }
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
    crazypod_popup_animate(panel, target_y);
}

static void begin_now_overlay(enum crazypod_now_playing_overlay overlay)
{
    bool replacing = now_overlay_root != NULL &&
        lv_obj_is_valid(now_overlay_root);

    destroy_now_volume_hud();
    if(replacing && overlay_host.preserve_modal_underlay != NULL)
        overlay_host.preserve_modal_underlay(overlay_host.context);
    destroy_now_overlay_objects();
    now_overlay = overlay;
    now_overlay_root = crazypod_ui_widget_box(
        overlay_host.parent, 0, 0,
        LCD_WIDTH, LCD_HEIGHT, 0,
        0x000000, LV_OPA_TRANSP);
    lv_obj_remove_flag(now_overlay_root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(now_overlay_root);
    if(overlay_host.create_modal_underlay != NULL)
        (void)overlay_host.create_modal_underlay(
            now_overlay_root, overlay_host.context);
}

static const char *now_playback_mode_label_for(
    enum now_playback_mode mode)
{
    if(mode == NOW_PLAYBACK_REPEAT_ONE)
        return CP_TR("REPEAT 1");
    if(mode == NOW_PLAYBACK_REPEAT_ALL)
        return CP_TR("REPEAT");
    if(mode == NOW_PLAYBACK_SHUFFLE)
        return CP_TR("SHUFFLE");
    return CP_TR("ORDERED");
}

static enum crazypod_ui_icon now_playback_mode_icon_for(
    enum now_playback_mode mode)
{
    if(mode == NOW_PLAYBACK_REPEAT_ONE)
        return CRAZYPOD_UI_ICON_REPEAT_ONE;
    if(mode == NOW_PLAYBACK_REPEAT_ALL)
        return CRAZYPOD_UI_ICON_REPEAT;
    if(mode == NOW_PLAYBACK_SHUFFLE)
        return CRAZYPOD_UI_ICON_SHUFFLE;
    return CRAZYPOD_UI_ICON_PLAY;
}

static enum now_playback_mode now_playback_mode_current(void)
{
    if(crazypod_queue_repeat() == REPEAT_ONE)
        return NOW_PLAYBACK_REPEAT_ONE;
    if(crazypod_queue_repeat() == REPEAT_ALL)
        return NOW_PLAYBACK_REPEAT_ALL;
    if(crazypod_queue_shuffle())
        return NOW_PLAYBACK_SHUFFLE;
    return NOW_PLAYBACK_ORDERED;
}

static const char *now_playback_mode_label(void)
{
    return now_playback_mode_label_for(
        now_playback_mode_current());
}

static enum crazypod_ui_icon now_playback_mode_icon(void)
{
    return now_playback_mode_icon_for(
        now_playback_mode_current());
}

static void retain_larger(int *value, int candidate)
{
    if(candidate > *value)
        *value = candidate;
}

static int now_actions_content_width(void)
{
    static const char *const labels[NOW_ACTION_CELL_COUNT] = {
        CP_TR("Favorite"), CP_TR("Playback"),
        CP_TR("Lyrics"), CP_TR("Progress")
    };
    const char *const details[] = {
        CP_TR("Scroll browse  Center play  Menu exits"),
        CP_TR("No track available"),
        CP_TR("Saved to My Favorites  Center removes"),
        CP_TR("Center adds to My Favorites"),
        CP_TR("Center adjusts track progress")
    };
    char playback[96];
    char lyrics[96];
    int longest_cell = 0;
    int width;
    int index;

    width = crazypod_popup_text_width(
        CP_TR("ACTIONS"), &lv_font_montserrat_10) + 28;
    retain_larger(
        &width,
        crazypod_popup_text_width(
            CP_TR("View Playback Queue"),
            &lv_font_montserrat_10) + 76);
    for(index = 0; index < NOW_ACTION_CELL_COUNT; ++index)
        retain_larger(
            &longest_cell,
            crazypod_popup_text_width(
                labels[index], &lv_font_montserrat_8));
    retain_larger(
        &width,
        32 + 7 * (NOW_ACTION_CELL_COUNT - 1) +
            NOW_ACTION_CELL_COUNT * (longest_cell + 6));
    for(index = 0;
        index < (int)(sizeof(details) / sizeof(details[0]));
        ++index) {
        retain_larger(
            &width,
            crazypod_popup_text_width(
                details[index], &lv_font_montserrat_8) + 28);
    }
    snprintf(
        playback, sizeof(playback), "%s  %s",
        CP_FMT("Play Mode"), now_playback_mode_label());
    retain_larger(
        &width,
        crazypod_popup_text_width(
            playback, &lv_font_montserrat_8) + 28);
    snprintf(
        lyrics, sizeof(lyrics), "%s  %s",
        CP_FMT("Lyrics visible"), CP_FMT("Local LRC"));
    retain_larger(
        &width,
        crazypod_popup_text_width(
            lyrics, &lv_font_montserrat_8) + 28);
    snprintf(
        lyrics, sizeof(lyrics), "%s  %s",
        CP_FMT("Lyrics hidden"), CP_FMT("No local LRC"));
    retain_larger(
        &width,
        crazypod_popup_text_width(
            lyrics, &lv_font_montserrat_8) + 28);
    return width;
}

static int now_actions_detail_height(int width)
{
    static const char *const details[] = {
        CP_TR("Scroll browse  Center play  Menu exits"),
        CP_TR("No track available"),
        CP_TR("Saved to My Favorites  Center removes"),
        CP_TR("Center adds to My Favorites"),
        CP_TR("Center adjusts track progress")
    };
    char composed[96];
    int height = 0;
    int index;

    for(index = 0;
        index < (int)(sizeof(details) / sizeof(details[0]));
        ++index) {
        retain_larger(
            &height,
            crazypod_popup_wrapped_text_height(
                details[index], &lv_font_montserrat_8,
                width, 1));
    }
    snprintf(
        composed, sizeof(composed), "%s  %s",
        CP_FMT("Play Mode"), now_playback_mode_label());
    retain_larger(
        &height,
        crazypod_popup_wrapped_text_height(
            composed, &lv_font_montserrat_8,
            width, 1));
    snprintf(
        composed, sizeof(composed), "%s  %s",
        CP_FMT("Lyrics visible"), CP_FMT("No local LRC"));
    retain_larger(
        &height,
        crazypod_popup_wrapped_text_height(
            composed, &lv_font_montserrat_8,
            width, 1));
    snprintf(
        composed, sizeof(composed), "%s  %s",
        CP_FMT("Lyrics hidden"), CP_FMT("Local LRC"));
    retain_larger(
        &height,
        crazypod_popup_wrapped_text_height(
            composed, &lv_font_montserrat_8,
            width, 1));
    return height;
}

static bool current_track_is_favorite(void)
{
    struct crazypod_track track;

    return copy_current_track(&track) &&
        crazypod_music_track_is_favorite(track.path);
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
        struct crazypod_track track;

        if(!copy_current_track(&track))
            snprintf(detail, sizeof(detail),
                     CP_FMT("No track available"));
        else
            snprintf(detail, sizeof(detail),
                     favorite
                         ? CP_FMT("Saved to My Favorites  Center removes")
                         : CP_FMT("Center adds to My Favorites"));
    }
    else if(now_action_selected == NOW_ACTION_PLAYBACK) {
        snprintf(detail, sizeof(detail), "%s  %s",
                 CP_FMT("Play Mode"), now_playback_mode_label());
    }
    else if(now_action_selected == NOW_ACTION_LYRICS) {
        struct crazypod_track track;
        bool available = copy_current_track(&track) &&
                         crazypod_lyrics_load(track.path);
        snprintf(detail, sizeof(detail), "%s  %s",
                 crazypod_state_lyrics_mode()
                     ? CP_FMT("Lyrics visible") : CP_FMT("Lyrics hidden"),
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
    struct crazypod_popup_geometry geometry;
    int row_width;
    int cells_width;
    int cell_width;
    int cell_gap = 7;
    int title_y = 12;
    int queue_y;
    int queue_height = 36;
    int cells_y;
    int cells_height = 52;
    int detail_height;
    int detail_y;
    int i;

    if(now_overlay == CRAZYPOD_NOW_OVERLAY_NONE)
        prepare_now_overlay_glass(true);
    begin_now_overlay(CRAZYPOD_NOW_OVERLAY_ACTIONS);
    if(now_action_selected < 0 ||
       now_action_selected >= NOW_ACTION_COUNT)
        now_action_selected = NOW_ACTION_QUEUE;

    geometry = crazypod_popup_centered_geometry(
        crazypod_popup_clamp_width(
            now_actions_content_width(),
            0, 250, LCD_WIDTH - 32),
        1);
    detail_height = now_actions_detail_height(
        geometry.width - 28);
    queue_y = title_y +
        lv_font_get_line_height(&lv_font_montserrat_10) + 9;
    cells_y = queue_y + queue_height + 8;
    detail_y = cells_y + cells_height + 7;
    geometry = crazypod_popup_centered_geometry(
        geometry.width, detail_y + detail_height + 9);
    row_width = geometry.width - 24;
    cells_width = geometry.width - 32;
    cell_width =
        (cells_width - cell_gap *
         (NOW_ACTION_CELL_COUNT - 1)) /
        NOW_ACTION_CELL_COUNT;
    now_overlay_panel = make_now_glass_panel(
        geometry.x, geometry.y,
        geometry.width, geometry.height);
    title = crazypod_ui_widget_label(
        now_overlay_panel, CP_TR("ACTIONS"),
        &lv_font_montserrat_10,
        COLOR_WHITE, 92);
    lv_obj_set_width(title, geometry.width - 28);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 14, title_y);

    now_actions_view.queue_row = crazypod_ui_widget_box(
        now_overlay_panel, 12, queue_y,
        row_width, queue_height, 10,
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
    lv_obj_set_pos(now_actions_view.queue_label, 34, 11);
    lv_obj_set_width(
        now_actions_view.queue_label, row_width - 68);
    lv_obj_set_style_text_align(
        now_actions_view.queue_label,
        LV_TEXT_ALIGN_LEFT, 0);
    chevron_box = crazypod_ui_widget_box(
        now_actions_view.queue_row,
        row_width - 25, 10, 16, 16, 0,
        COLOR_WHITE, LV_OPA_TRANSP);
    chevron = crazypod_ui_widget_label(
        chevron_box, LV_SYMBOL_RIGHT,
        &lv_font_montserrat_8, COLOR_WHITE, 110);
    lv_obj_center(chevron);

    for(i = 0; i < NOW_ACTION_CELL_COUNT; ++i) {
        static const char *const labels[NOW_ACTION_CELL_COUNT] = {
            CP_TR("Favorite"), CP_TR("Playback"),
            CP_TR("Lyrics"), CP_TR("Progress")
        };
        static const enum crazypod_ui_icon icons[NOW_ACTION_CELL_COUNT] = {
            CRAZYPOD_UI_ICON_HEART,
            CRAZYPOD_UI_ICON_PLAY,
            CRAZYPOD_UI_ICON_FILE,
            CRAZYPOD_UI_ICON_BARS
        };
        int x = 16 + i * (cell_width + cell_gap);

        now_actions_view.cells[i] = crazypod_ui_widget_box(
            now_overlay_panel, x, cells_y,
            cell_width, cells_height, 14,
            COLOR_WHITE, 10);
        now_actions_view.cell_icons[i] =
            crazypod_ui_widget_icon(
                now_actions_view.cells[i],
                (cell_width - 16) / 2, 7,
                icons[i], COLOR_WHITE, 124);
        now_actions_view.cell_labels[i] = crazypod_ui_widget_label(
            now_actions_view.cells[i], labels[i],
            &lv_font_montserrat_8,
            COLOR_WHITE, 120);
        lv_obj_set_width(
            now_actions_view.cell_labels[i], cell_width);
        lv_obj_set_style_text_align(
            now_actions_view.cell_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(now_actions_view.cell_labels[i], 0, 32);
    }

    now_actions_view.detail = crazypod_ui_widget_label(
        now_overlay_panel, "",
        &lv_font_montserrat_8,
        COLOR_WHITE, 174);
    lv_obj_set_width(
        now_actions_view.detail, geometry.width - 28);
    lv_obj_set_height(
        now_actions_view.detail, detail_height);
    lv_obj_set_style_text_align(
        now_actions_view.detail, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_long_mode(
        now_actions_view.detail, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_line_space(
        now_actions_view.detail, 1, 0);
    lv_obj_set_pos(now_actions_view.detail, 14, detail_y);
    refresh_now_actions_popup();
    animate_now_popup(now_overlay_panel, geometry.y);
}

static int now_playback_popup_width(void)
{
    int width;
    int mode;

    width = crazypod_popup_text_width(
        CP_TR("Play Mode"), &lv_font_montserrat_10) + 28;
    retain_larger(
        &width,
        crazypod_popup_text_width(
            CP_TR("Scroll previews  Center applies  Menu cancels"),
            &lv_font_montserrat_8) + 28);
    for(mode = 0; mode < NOW_PLAYBACK_MODE_COUNT; ++mode) {
        retain_larger(
            &width,
            crazypod_popup_text_width(
                now_playback_mode_label_for(
                    (enum now_playback_mode)mode),
                &lv_font_montserrat_10) + 76);
    }
    return crazypod_popup_clamp_width(
        width, 0, 184, LCD_WIDTH - 32);
}

static void refresh_now_playback_popup(void)
{
    enum now_playback_mode current =
        now_playback_mode_current();
    int mode;

    for(mode = 0; mode < NOW_PLAYBACK_MODE_COUNT; ++mode) {
        bool selected = mode == now_playback_selected;
        bool active = mode == (int)current;

        lv_obj_set_style_bg_color(
            now_playback_view.rows[mode],
            lv_color_hex(selected ? 0xDBD1BD : COLOR_WHITE), 0);
        lv_obj_set_style_bg_opa(
            now_playback_view.rows[mode],
            selected ? 38 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(
            now_playback_view.rows[mode], 1, 0);
        lv_obj_set_style_border_color(
            now_playback_view.rows[mode],
            lv_color_hex(selected ? 0xDBD1BD : COLOR_WHITE), 0);
        lv_obj_set_style_border_opa(
            now_playback_view.rows[mode], selected ? 154 : 20, 0);
        crazypod_ui_widget_icon_set_color(
            now_playback_view.icons[mode],
            active ? COLOR_CYAN : COLOR_WHITE);
        lv_obj_set_style_opa(
            now_playback_view.icons[mode],
            selected ? LV_OPA_COVER : active ? 220 : 118, 0);
        lv_obj_set_style_text_opa(
            now_playback_view.labels[mode],
            selected ? LV_OPA_COVER : active ? 225 : 150, 0);
        CP_LV_LABEL_SET_TEXT(
            now_playback_view.markers[mode],
            active ? LV_SYMBOL_OK : "");
    }
}

static void show_now_playback_popup(void)
{
    lv_obj_t *title;
    lv_obj_t *instruction;
    struct crazypod_popup_geometry geometry;
    int title_y = 12;
    int rows_y;
    int instruction_y;
    int instruction_height;
    int row_width;
    int mode;

    if(now_overlay == CRAZYPOD_NOW_OVERLAY_NONE)
        prepare_now_overlay_glass(true);
    begin_now_overlay(CRAZYPOD_NOW_OVERLAY_PLAYBACK);
    if(now_playback_selected < 0 ||
       now_playback_selected >= NOW_PLAYBACK_MODE_COUNT)
        now_playback_selected = now_playback_mode_current();
    geometry = crazypod_popup_centered_geometry(
        now_playback_popup_width(), 1);
    instruction_height = crazypod_popup_wrapped_text_height(
        CP_TR("Scroll previews  Center applies  Menu cancels"),
        &lv_font_montserrat_8,
        geometry.width - 28, 1);
    rows_y = title_y +
        lv_font_get_line_height(&lv_font_montserrat_10) + 8;
    instruction_y = rows_y +
        NOW_PLAYBACK_MODE_COUNT * NOW_PLAYBACK_ROW_HEIGHT + 8;
    geometry = crazypod_popup_centered_geometry(
        geometry.width,
        instruction_y + instruction_height + 10);
    row_width = geometry.width - 24;
    now_overlay_panel = make_now_glass_panel(
        geometry.x, geometry.y,
        geometry.width, geometry.height);
    title = crazypod_ui_widget_label(
        now_overlay_panel, CP_TR("Play Mode"),
        &lv_font_montserrat_10, COLOR_WHITE, 110);
    lv_obj_set_pos(title, 14, title_y);
    lv_obj_set_width(title, geometry.width - 28);
    lv_obj_set_style_text_align(
        title, LV_TEXT_ALIGN_CENTER, 0);

    for(mode = 0; mode < NOW_PLAYBACK_MODE_COUNT; ++mode) {
        int y = rows_y + mode * NOW_PLAYBACK_ROW_HEIGHT;

        now_playback_view.rows[mode] = crazypod_ui_widget_box(
            now_overlay_panel, 12, y,
            row_width, NOW_PLAYBACK_ROW_HEIGHT, 7,
            COLOR_WHITE, LV_OPA_TRANSP);
        now_playback_view.icons[mode] = crazypod_ui_widget_icon(
            now_playback_view.rows[mode], 9, 7,
            now_playback_mode_icon_for(
                (enum now_playback_mode)mode),
            COLOR_WHITE, 118);
        now_playback_view.labels[mode] =
            crazypod_ui_widget_label(
                now_playback_view.rows[mode],
                now_playback_mode_label_for(
                    (enum now_playback_mode)mode),
                &lv_font_montserrat_10,
                COLOR_WHITE, 150);
        lv_obj_set_pos(now_playback_view.labels[mode], 36, 10);
        lv_obj_set_width(
            now_playback_view.labels[mode], row_width - 70);
        now_playback_view.markers[mode] =
            crazypod_ui_widget_label(
                now_playback_view.rows[mode], "",
                &lv_font_montserrat_10,
                COLOR_CYAN, LV_OPA_COVER);
        lv_obj_set_pos(
            now_playback_view.markers[mode],
            row_width - 27, 10);
        lv_obj_set_width(now_playback_view.markers[mode], 18);
        lv_obj_set_style_text_align(
            now_playback_view.markers[mode],
            LV_TEXT_ALIGN_CENTER, 0);
    }

    instruction = crazypod_ui_widget_label(
        now_overlay_panel,
        CP_TR("Scroll previews  Center applies  Menu cancels"),
        &lv_font_montserrat_8,
        COLOR_WHITE, 145);
    lv_obj_set_pos(instruction, 14, instruction_y);
    lv_obj_set_size(
        instruction, geometry.width - 28, instruction_height);
    lv_label_set_long_mode(
        instruction, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(
        instruction, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_line_space(instruction, 1, 0);
    refresh_now_playback_popup();
    animate_now_popup(now_overlay_panel, geometry.y);
}

static int now_queue_start(int count)
{
    int start;

    if(count <= 3)
        return 0;
    start = now_queue_selected - 1;
    if(start < 0)
        start = 0;
    if(start > count - 3)
        start = count - 3;
    return start;
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

    start = now_queue_start(count);
    for(row = 0; row < 3; ++row) {
        int index = start + row;
        bool selected = index == now_queue_selected;
        bool current_row = index == current;
        char path[MAX_PATH];
        struct crazypod_track track;
        bool have_track;

        if(index < 0 || index >= count) {
            crazypod_marquee_configure(
                now_queue_view.titles[row], false);
            lv_obj_add_flag(now_queue_view.rows[row],
                            LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(now_queue_view.rows[row],
                           LV_OBJ_FLAG_HIDDEN);
        have_track = crazypod_queue_copy_path(
                index, path, sizeof(path)) &&
            crazypod_music_copy_track(
                crazypod_music_find_track(path), &track);
        crazypod_marquee_set_text(
            now_queue_view.titles[row],
            have_track ? track.title : CP_TR("Unavailable"),
            selected);
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
    }
    now_queue_generation_seen = crazypod_queue_generation();
}

static int now_queue_visible_rows(int count)
{
    if(count <= 0)
        return 0;
    return count < 3 ? count : 3;
}

static void show_now_queue_popup(void)
{
    struct crazypod_popup_geometry geometry;
    int count;
    int visible_rows;
    int empty_height;
    int row_width;
    int row;

    if(now_overlay == CRAZYPOD_NOW_OVERLAY_NONE)
        prepare_now_overlay_glass(false);
    begin_now_overlay(CRAZYPOD_NOW_OVERLAY_QUEUE);
    count = crazypod_queue_count();
    if(count > 0 &&
       (now_queue_selected < 0 ||
        now_queue_selected >= count))
        now_queue_selected = crazypod_queue_index();
    if(now_queue_selected < 0)
        now_queue_selected = 0;
    if(count > 0 && now_queue_selected >= count)
        now_queue_selected = count - 1;
    visible_rows = now_queue_visible_rows(count);
    empty_height = lv_font_get_line_height(CRAZYPOD_METADATA_FONT);
    geometry = crazypod_popup_centered_geometry(
        NOW_QUEUE_POPUP_WIDTH,
        visible_rows > 0
            ? NOW_QUEUE_HEADER_HEIGHT +
                visible_rows * NOW_QUEUE_ROW_HEIGHT + 8
            : NOW_QUEUE_HEADER_HEIGHT + empty_height + 18);
    row_width = geometry.width - 28;
    now_overlay_panel = make_now_glass_panel(
        geometry.x, geometry.y,
        geometry.width, geometry.height);
    now_queue_view.mode_icon = crazypod_ui_widget_icon(
        now_overlay_panel, 14, 8,
        now_playback_mode_icon(), COLOR_WHITE, LV_OPA_COVER);
    now_queue_view.mode = crazypod_ui_widget_label(
        now_overlay_panel, "",
        &lv_font_montserrat_10,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(now_queue_view.mode, 36, 12);
    lv_obj_set_width(
        now_queue_view.mode, geometry.width - 72);
    lv_obj_set_style_text_align(
        now_queue_view.mode, LV_TEXT_ALIGN_CENTER, 0);
    now_queue_view.count = crazypod_ui_widget_label(
        now_overlay_panel, "0/0",
        &lv_font_montserrat_8,
        COLOR_WHITE, LV_OPA_COVER);
    lv_obj_set_width(now_queue_view.count, 54);
    lv_obj_set_style_text_align(
        now_queue_view.count, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(
        now_queue_view.count,
        geometry.width - 68, 14);

    now_queue_view.empty = crazypod_ui_widget_label(
        now_overlay_panel, CP_TR("No Queue"),
        CRAZYPOD_METADATA_FONT,
        COLOR_WHITE, 225);
    lv_obj_set_width(now_queue_view.empty, row_width);
    lv_obj_set_style_text_align(
        now_queue_view.empty, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(
        now_queue_view.empty, 14,
        NOW_QUEUE_HEADER_HEIGHT +
            (geometry.height - NOW_QUEUE_HEADER_HEIGHT -
             empty_height) / 2);

    for(row = 0; row < 3; ++row) {
        int y = NOW_QUEUE_HEADER_HEIGHT +
            row * NOW_QUEUE_ROW_HEIGHT;

        now_queue_view.rows[row] = crazypod_ui_widget_box(
            now_overlay_panel, 14, y,
            row_width, NOW_QUEUE_ROW_HEIGHT, 6,
            COLOR_WHITE, LV_OPA_TRANSP);
        now_queue_view.icons[row] = crazypod_ui_widget_label(
            now_queue_view.rows[row], LV_SYMBOL_BULLET,
            &lv_font_montserrat_10,
            COLOR_WHITE, 75);
        lv_obj_set_size(now_queue_view.icons[row], 18, 12);
        lv_obj_set_style_text_align(
            now_queue_view.icons[row], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(
            now_queue_view.icons[row], 7,
            (NOW_QUEUE_ROW_HEIGHT - 12) / 2);
        now_queue_view.titles[row] = crazypod_ui_widget_label(
            now_queue_view.rows[row], "",
            NOW_QUEUE_TITLE_FONT,
            COLOR_WHITE, 220);
        lv_obj_set_width(
            now_queue_view.titles[row], row_width - 41);
        lv_obj_set_style_text_align(
            now_queue_view.titles[row],
            LV_TEXT_ALIGN_LEFT, 0);
        crazypod_ui_widget_align_row_label(
            now_queue_view.titles[row], 32,
            CRAZYPOD_UI_ROW_LABEL_TEXT);
        crazypod_marquee_configure(
            now_queue_view.titles[row], false);
    }
    refresh_now_queue_popup();
    animate_now_popup(now_overlay_panel, geometry.y);
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

static void remember_now_progress_track(void)
{
    if(!crazypod_queue_copy_path(
           crazypod_queue_index(), now_progress_track_path,
           sizeof(now_progress_track_path)))
        now_progress_track_path[0] = '\0';
}

static bool now_progress_track_matches(void)
{
    char path[MAX_PATH];

    return crazypod_queue_copy_path(
               crazypod_queue_index(), path, sizeof(path)) &&
        strcmp(now_progress_track_path, path) == 0;
}

static bool now_progress_available(void)
{
    const struct mp3entry *id3 = audio_current_track();

    return id3 != NULL && id3->length > 0;
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
            (uint64_t)now_progress_view.fill_max_width *
            now_progress_elapsed_ms /
            now_progress_length_ms);
    }
    if(width > now_progress_view.fill_max_width)
        width = now_progress_view.fill_max_width;
    lv_obj_set_width(now_progress_view.fill, width);
    format_now_progress_time(
        now_progress_elapsed_ms, elapsed, sizeof(elapsed));
    format_now_progress_time(
        now_progress_length_ms, duration, sizeof(duration));
    CP_LV_LABEL_SET_TEXT(now_progress_view.elapsed, elapsed);
    CP_LV_LABEL_SET_TEXT(now_progress_view.duration, duration);
}

static void show_now_progress_popup(bool begin_session)
{
    lv_obj_t *title;
    lv_obj_t *track;
    lv_obj_t *instruction;
    struct crazypod_popup_geometry geometry;
    int title_y = 14;
    int track_y;
    int time_y;
    int instruction_y;
    int instruction_height;
    int track_width;

    if(now_overlay == CRAZYPOD_NOW_OVERLAY_NONE)
        prepare_now_overlay_glass(true);
    begin_now_overlay(CRAZYPOD_NOW_OVERLAY_PROGRESS);
    if(begin_session) {
        sync_now_progress_from_playback();
        remember_now_progress_track();
        now_progress_dirty = false;
    }
    geometry = crazypod_popup_centered_geometry(
        crazypod_popup_clamp_width(
            crazypod_popup_text_width(
                CP_TR(
                    "Scroll previews  Center applies  Menu cancels"),
                &lv_font_montserrat_8),
            22, 220, LCD_WIDTH - 32),
        1);
    instruction_height = crazypod_popup_wrapped_text_height(
        CP_TR("Scroll previews  Center applies  Menu cancels"),
        &lv_font_montserrat_8,
        geometry.width - 28, 1);
    track_y = title_y +
        lv_font_get_line_height(&lv_font_montserrat_10) + 25;
    time_y = track_y + 20;
    instruction_y = time_y +
        lv_font_get_line_height(&lv_font_montserrat_10) + 12;
    geometry = crazypod_popup_centered_geometry(
        geometry.width,
        instruction_y + instruction_height + 8);
    track_width = geometry.width - 70;
    now_progress_view.fill_max_width = track_width - 2;
    now_overlay_panel = make_now_glass_panel(
        geometry.x, geometry.y,
        geometry.width, geometry.height);
    title = crazypod_ui_widget_label(now_overlay_panel, CP_TR("PROGRESS"),
                       &lv_font_montserrat_10,
                       COLOR_WHITE, 100);
    lv_obj_set_width(title, geometry.width - 28);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 14, title_y);
    now_progress_view.icon = crazypod_ui_widget_icon(
        now_overlay_panel, 22, track_y - 3,
        CRAZYPOD_UI_ICON_BARS, COLOR_CYAN, LV_OPA_COVER);
    track = crazypod_ui_widget_box(
        now_overlay_panel, 53, track_y,
        track_width, 10,
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
    lv_obj_set_width(
        now_progress_view.elapsed, track_width / 2);
    lv_obj_set_pos(now_progress_view.elapsed, 53, time_y);
    now_progress_view.duration = crazypod_ui_widget_label(
        now_overlay_panel, "0:00",
        &lv_font_montserrat_10,
        COLOR_WHITE, 235);
    lv_obj_set_width(
        now_progress_view.duration, track_width / 2);
    lv_obj_set_style_text_align(
        now_progress_view.duration, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(
        now_progress_view.duration,
        53 + track_width / 2, time_y);
    instruction = crazypod_ui_widget_label(
        now_overlay_panel,
        CP_TR("Scroll previews  Center applies  Menu cancels"),
        &lv_font_montserrat_8,
        COLOR_WHITE, 145);
    lv_obj_set_width(instruction, geometry.width - 28);
    lv_obj_set_height(instruction, instruction_height);
    lv_obj_set_style_text_align(
        instruction, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_long_mode(
        instruction, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_line_space(instruction, 1, 0);
    lv_obj_set_pos(instruction, 14, instruction_y);
    refresh_now_progress_popup();
    animate_now_popup(now_overlay_panel, geometry.y);
}

static void restore_now_overlay(enum crazypod_now_playing_overlay overlay)
{
    if(overlay == CRAZYPOD_NOW_OVERLAY_ACTIONS)
        show_now_actions_popup();
    else if(overlay == CRAZYPOD_NOW_OVERLAY_PLAYBACK)
        show_now_playback_popup();
    else if(overlay == CRAZYPOD_NOW_OVERLAY_QUEUE)
        show_now_queue_popup();
    else if(overlay == CRAZYPOD_NOW_OVERLAY_PROGRESS)
        show_now_progress_popup(false);
}

static void dismiss_now_overlay(bool refresh_now_playing)
{
    lv_obj_t *root = now_overlay_root;
    lv_display_t *display = NULL;

    if(root != NULL && lv_obj_is_valid(root)) {
        display = lv_obj_get_display(root);
        if(now_overlay_panel != NULL &&
           lv_obj_is_valid(now_overlay_panel))
            lv_anim_delete(now_overlay_panel, NULL);
        lv_obj_delete(root);
    }
    clear_now_overlay_objects();
    now_overlay = CRAZYPOD_NOW_OVERLAY_NONE;
    if(display != NULL)
        lv_refr_now(display);
    if(overlay_host.teardown_complete != NULL)
        overlay_host.teardown_complete(overlay_host.context);
    if(refresh_now_playing && overlay_host.render != NULL)
        overlay_host.render(overlay_host.context);
}

static void notify_now_overlay(
    const char *message, bool success)
{
    if(overlay_host.notify != NULL)
        overlay_host.notify(message, success);
}

static void dismiss_now_overlay_with_notice(
    const char *message, bool success, bool refresh)
{
    dismiss_now_overlay(refresh);
    notify_now_overlay(message, success);
}

static bool apply_now_playback_mode(
    enum now_playback_mode mode)
{
    bool target_shuffle = mode == NOW_PLAYBACK_SHUFFLE;
    int target_repeat = REPEAT_OFF;
    bool changed = false;

    if(mode == NOW_PLAYBACK_REPEAT_ALL)
        target_repeat = REPEAT_ALL;
    else if(mode == NOW_PLAYBACK_REPEAT_ONE)
        target_repeat = REPEAT_ONE;

    if(target_shuffle) {
        if(crazypod_queue_repeat() != REPEAT_OFF) {
            crazypod_queue_set_repeat(REPEAT_OFF);
            changed = true;
        }
        if(!crazypod_queue_shuffle()) {
            crazypod_queue_set_shuffle(true);
            changed = true;
        }
    }
    else {
        if(crazypod_queue_shuffle()) {
            crazypod_queue_set_shuffle(false);
            changed = true;
        }
        if(crazypod_queue_repeat() != target_repeat) {
            crazypod_queue_set_repeat(target_repeat);
            changed = true;
        }
    }
    if(changed)
        crazypod_state_mark_dirty();
    return changed;
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
    destroy_now_overlay_objects();
    now_overlay = CRAZYPOD_NOW_OVERLAY_NONE;
    now_progress_dirty = false;
    now_progress_track_path[0] = '\0';
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
    return crazypod_state_lyrics_mode();
}

void crazypod_now_playing_overlay_show_actions(void)
{
    now_action_selected = NOW_ACTION_QUEUE;
    show_now_actions_popup();
}

void crazypod_now_playing_overlay_show_queue(void)
{
    show_now_queue_popup();
}

void crazypod_now_playing_overlay_show_progress(void)
{
    if(!now_progress_available()) {
        dismiss_now_overlay_with_notice(
            CP_TR("No track available"), false, true);
        return;
    }
    show_now_progress_popup(true);
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
    enum now_playback_mode next =
        (enum now_playback_mode)(
            (now_playback_mode_current() + 1) %
            NOW_PLAYBACK_MODE_COUNT);

    (void)apply_now_playback_mode(next);
    if(now_overlay == CRAZYPOD_NOW_OVERLAY_ACTIONS)
        refresh_now_actions_popup();
    else if(now_overlay == CRAZYPOD_NOW_OVERLAY_PLAYBACK)
        refresh_now_playback_popup();
    else if(now_overlay == CRAZYPOD_NOW_OVERLAY_QUEUE)
        refresh_now_queue_popup();
    else if(overlay_host.render != NULL)
        overlay_host.render(overlay_host.context);
}

static void commit_now_progress(void);

void crazypod_now_playing_overlay_activate(void)
{
    if(now_overlay == CRAZYPOD_NOW_OVERLAY_ACTIONS) {
        if(now_action_selected == NOW_ACTION_QUEUE) {
            now_queue_selected = crazypod_queue_count() > 0
                ? crazypod_queue_index() : 0;
            show_now_queue_popup();
        }
        else if(now_action_selected == NOW_ACTION_FAVORITE) {
            struct crazypod_track track;
            bool favorite;

            if(!copy_current_track(&track)) {
                dismiss_now_overlay_with_notice(
                    CP_TR("No track available"), false, true);
                return;
            }
            favorite = current_track_is_favorite();
            if(!crazypod_music_toggle_favorite(track.path)) {
                dismiss_now_overlay_with_notice(
                    CP_TR("Favorite Save Failed"), false, true);
                return;
            }
            dismiss_now_overlay_with_notice(
                favorite
                    ? CP_TR("Removed from Favorites")
                    : CP_TR("Saved to Favorites"),
                true, true);
        }
        else if(now_action_selected == NOW_ACTION_PLAYBACK) {
            now_playback_selected = now_playback_mode_current();
            show_now_playback_popup();
        }
        else if(now_action_selected == NOW_ACTION_LYRICS) {
            bool lyrics_mode = !crazypod_state_lyrics_mode();

            crazypod_state_set_lyrics_mode(lyrics_mode);
            dismiss_now_overlay_with_notice(
                lyrics_mode
                    ? CP_TR("Lyrics visible")
                    : CP_TR("Lyrics hidden"),
                true, true);
        }
        else
            crazypod_now_playing_overlay_show_progress();
        return;
    }
    if(now_overlay == CRAZYPOD_NOW_OVERLAY_PLAYBACK) {
        enum now_playback_mode selected =
            (enum now_playback_mode)now_playback_selected;

        (void)apply_now_playback_mode(selected);
        dismiss_now_overlay_with_notice(
            now_playback_mode_label_for(selected), true, true);
        return;
    }
    if(now_overlay == CRAZYPOD_NOW_OVERLAY_QUEUE) {
        if(now_queue_selected >= 0 &&
           now_queue_selected < crazypod_queue_count()) {
            if(overlay_host.select_queue_item != NULL)
                overlay_host.select_queue_item(now_queue_selected);
            crazypod_state_forget_resume();
            crazypod_state_mark_dirty();
            dismiss_now_overlay(true);
        }
        return;
    }
    if(now_overlay == CRAZYPOD_NOW_OVERLAY_PROGRESS)
        commit_now_progress();
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
        iap_on_volume(volume);
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
    now_progress_dirty = true;
    refresh_now_progress_popup();
}

static void commit_now_progress(void)
{
    char time[16];
    char notice[64];

    if(!now_progress_track_matches()) {
        dismiss_now_overlay(true);
        return;
    }
    if(!now_progress_available()) {
        dismiss_now_overlay_with_notice(
            CP_TR("No track available"), false, true);
        return;
    }
    if(!now_progress_dirty) {
        dismiss_now_overlay(true);
        return;
    }

    audio_pre_ff_rewind();
    audio_ff_rewind((long)now_progress_elapsed_ms);
    crazypod_state_mark_dirty();
    format_now_progress_time(
        now_progress_elapsed_ms, time, sizeof(time));
    snprintf(
        notice, sizeof(notice),
        CP_FMT("Jumped to %s"), time);
    dismiss_now_overlay_with_notice(notice, true, true);
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
            refresh_now_queue_popup();
        }
    }
    else if(now_overlay == CRAZYPOD_NOW_OVERLAY_PLAYBACK) {
        int next = now_playback_selected + direction;

        if(next < 0)
            next = 0;
        if(next >= NOW_PLAYBACK_MODE_COUNT)
            next = NOW_PLAYBACK_MODE_COUNT - 1;
        if(next != now_playback_selected) {
            now_playback_selected = next;
            refresh_now_playback_popup();
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

static void refresh_now_progress_after_playback(void)
{
    if(now_overlay != CRAZYPOD_NOW_OVERLAY_PROGRESS)
        return;
    if(!now_progress_track_matches() ||
       !now_progress_available()) {
        dismiss_now_overlay(true);
        return;
    }
    if(now_progress_dirty)
        return;
    sync_now_progress_from_playback();
    refresh_now_progress_popup();
}

void crazypod_now_playing_overlay_refresh_after_playback(void)
{
    crazypod_now_playing_overlay_refresh_queue();
    refresh_now_progress_after_playback();
}

void crazypod_now_playing_overlay_refresh_tick(void)
{
    if(now_overlay == CRAZYPOD_NOW_OVERLAY_QUEUE &&
       now_queue_generation_seen != crazypod_queue_generation())
        show_now_queue_popup();
    else
        refresh_now_progress_after_playback();
}

#endif
