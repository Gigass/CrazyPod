#include "config.h"

#include "../../../crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdio.h>
#include <string.h>

#include "button.h"
#include "file.h"

#include "../../../crazypod_artwork.h"
#include "../../../crazypod_miniapps.h"
#include "../miniapps/crazypod_miniapps_feature.h"
#include "crazypod_now_playing_feature.h"

#define THEME_CONFIG "/.crazypod/now-playing-theme.cfg"
#define THEME_CONFIG_TEMP "/.crazypod/now-playing-theme.tmp"

static struct {
    char active_id[CRAZYPOD_MINIAPP_ID_SIZE];
    bool prepared;
    bool open_failed;
    int last_error;
} theme;

static int artwork_source_size(
    const struct crazypod_miniapp_metadata *metadata)
{
    return metadata != NULL && metadata->artwork_source_size > 0
        ? metadata->artwork_source_size
        : CRAZYPOD_ARTWORK_CACHE_SIZE;
}

static const struct crazypod_miniapp_metadata *active_metadata(void)
{
    int index;

    if(theme.active_id[0] == '\0')
        return NULL;
    index = crazypod_now_playing_themes_find(theme.active_id);
    return index >= 0
        ? crazypod_now_playing_themes_metadata(index) : NULL;
}

static bool write_exact(int fd, const void *buffer, size_t size)
{
    const uint8_t *cursor = buffer;

    while(size > 0) {
        ssize_t count = write(fd, cursor, size);

        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static bool persist_id(const char *id)
{
    int fd;
    size_t length;
    bool valid;

    if(id == NULL || id[0] == '\0') {
        (void)remove(THEME_CONFIG_TEMP);
        return remove(THEME_CONFIG) == 0 || !file_exists(THEME_CONFIG);
    }
    length = strlen(id);
    fd = open(THEME_CONFIG_TEMP,
              O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    valid = write_exact(fd, id, length) &&
        write_exact(fd, "\n", 1u) && fsync(fd) == 0;
    if(close(fd) < 0)
        valid = false;
    if(!valid || rename(THEME_CONFIG_TEMP, THEME_CONFIG) < 0) {
        (void)remove(THEME_CONFIG_TEMP);
        return false;
    }
    return true;
}

static void load_active_id(void)
{
    int fd = open(THEME_CONFIG, O_RDONLY);
    ssize_t count;

    theme.active_id[0] = '\0';
    if(fd < 0)
        return;
    count = read(fd, theme.active_id, sizeof(theme.active_id) - 1u);
    close(fd);
    if(count <= 0 || count >= (ssize_t)sizeof(theme.active_id)) {
        theme.active_id[0] = '\0';
        return;
    }
    theme.active_id[count] = '\0';
    while(count > 0 &&
          (theme.active_id[count - 1] == '\n' ||
           theme.active_id[count - 1] == '\r'))
        theme.active_id[--count] = '\0';
}

void crazypod_now_playing_theme_prepare(void)
{
    if(theme.prepared)
        return;
    load_active_id();
    if(theme.active_id[0] != '\0' &&
       (crazypod_miniapps_prepare() < 0 ||
        crazypod_now_playing_themes_find(theme.active_id) < 0))
        theme.active_id[0] = '\0';
    crazypod_now_playing_artwork_set_source_size(
        artwork_source_size(active_metadata()));
    theme.prepared = true;
    theme.open_failed = false;
    theme.last_error = CRAZYPOD_MINIAPP_OK;
}

int crazypod_now_playing_theme_choice_count(void)
{
    crazypod_now_playing_theme_prepare();
    (void)crazypod_miniapps_prepare();
    return crazypod_now_playing_themes_count() + 1;
}

const char *crazypod_now_playing_theme_choice_title(int index)
{
    const struct crazypod_miniapp_metadata *metadata;

    if(index == 0)
        return CP_TR("Default");
    metadata = crazypod_now_playing_themes_metadata(index - 1);
    return metadata != NULL ? metadata->name : "";
}

bool crazypod_now_playing_theme_choice_current(int index)
{
    const struct crazypod_miniapp_metadata *metadata;

    crazypod_now_playing_theme_prepare();
    if(index == 0)
        return theme.active_id[0] == '\0';
    metadata = crazypod_now_playing_themes_metadata(index - 1);
    return metadata != NULL &&
        strcmp(metadata->id, theme.active_id) == 0;
}

bool crazypod_now_playing_theme_select(int index)
{
    const struct crazypod_miniapp_metadata *metadata = NULL;
    const char *next = "";

    crazypod_now_playing_theme_prepare();
    if(crazypod_miniapps_prepare() < 0)
        return false;
    if(index > 0) {
        metadata = crazypod_now_playing_themes_metadata(index - 1);
        if(metadata == NULL)
            return false;
        next = metadata->id;
    }
    if(!persist_id(next))
        return false;
    if(crazypod_now_playing_theme_open())
        crazypod_now_playing_theme_close();
    snprintf(theme.active_id, sizeof(theme.active_id), "%s", next);
    crazypod_now_playing_artwork_set_source_size(
        artwork_source_size(metadata));
    theme.open_failed = false;
    theme.last_error = CRAZYPOD_MINIAPP_OK;
    return true;
}

bool crazypod_now_playing_theme_enabled(void)
{
    crazypod_now_playing_theme_prepare();
    return theme.active_id[0] != '\0' && !theme.open_failed;
}

bool crazypod_now_playing_theme_open(void)
{
    return crazypod_miniapps_is_open() &&
        crazypod_miniapps_current_kind() ==
            CRAZYPOD_MINIAPP_KIND_NOW_PLAYING_THEME;
}

bool crazypod_now_playing_theme_modal_visible(void)
{
    return crazypod_now_playing_theme_open() &&
        crazypod_miniapps_feature_modal_visible();
}

bool crazypod_now_playing_theme_owns_status_bar(void)
{
    const struct crazypod_miniapp_metadata *metadata;

    if(!crazypod_now_playing_theme_open())
        return false;
    metadata = crazypod_miniapps_current_metadata();
    return metadata != NULL &&
        metadata->status_bar == CRAZYPOD_MINIAPP_STATUS_BAR_THEME;
}

int crazypod_now_playing_theme_last_error(void)
{
    return theme.last_error;
}

static bool ensure_open(void)
{
    const struct crazypod_miniapp_metadata *metadata;
    int result;

    if(!crazypod_now_playing_theme_enabled())
        return false;
    if(crazypod_now_playing_theme_open()) {
        metadata = crazypod_miniapps_current_metadata();
        return metadata != NULL &&
            strcmp(metadata->id, theme.active_id) == 0;
    }
    if(crazypod_miniapps_is_open())
        return false;
    result = crazypod_now_playing_themes_open_id(theme.active_id);
    if(result != CRAZYPOD_MINIAPP_OK) {
        theme.open_failed = true;
        theme.last_error = result;
        crazypod_now_playing_artwork_set_source_size(
            CRAZYPOD_ARTWORK_CACHE_SIZE);
        return false;
    }
    theme.last_error = CRAZYPOD_MINIAPP_OK;
    crazypod_miniapps_feature_note_opened();
    return true;
}

bool crazypod_now_playing_theme_render(
    lv_obj_t *parent, uint32_t accent)
{
    if(!ensure_open())
        return false;
    crazypod_miniapps_feature_render_active(parent, accent);
    return true;
}

static bool translate_input(
    const struct crazypod_input_event *input,
    struct cp_input_event *output)
{
    memset(output, 0, sizeof(*output));
    output->struct_size = sizeof(*output);
    output->steps = 1;
    output->repeated = input->repeated ? 1 : 0;
    if(input->base == BUTTON_SCROLL_FWD) {
        output->type = CP_INPUT_WHEEL_CLOCKWISE;
        output->steps =
            (uint8_t)crazypod_input_wheel_steps(input, 4);
    }
    else if(input->base == BUTTON_SCROLL_BACK) {
        output->type = CP_INPUT_WHEEL_COUNTERCLOCKWISE;
        output->steps =
            (uint8_t)crazypod_input_wheel_steps(input, 4);
    }
    else if(input->base == BUTTON_LEFT)
        output->type = CP_INPUT_LEFT;
    else if(input->base == BUTTON_RIGHT)
        output->type = CP_INPUT_RIGHT;
    else if(input->base == BUTTON_SELECT)
        output->type = CP_INPUT_SELECT;
    else if(input->base == BUTTON_PLAY)
        output->type = CP_INPUT_PLAY;
    else if(input->base == BUTTON_MENU)
        output->type = CP_INPUT_MENU;
    else
        return false;
    return true;
}

bool crazypod_now_playing_theme_handle_input(
    const struct crazypod_input_event *event)
{
    struct cp_input_event native_event;

    if(!crazypod_now_playing_theme_open() || event == NULL)
        return false;
    if(event->base == BUTTON_MENU &&
       !crazypod_miniapps_feature_modal_visible())
        return false;
    if(event->release)
        return true;
    if(!translate_input(event, &native_event))
        return false;
    if(native_event.type == CP_INPUT_WHEEL_CLOCKWISE ||
       native_event.type == CP_INPUT_WHEEL_COUNTERCLOCKWISE) {
        /* Preserve wheel speed while keeping at most one pending gesture. */
        crazypod_miniapps_feature_push_wheel_coalesced(&native_event);
        return true;
    }
    if(event->repeated)
        return true;
    /* Buttons must act on the selection already presented on screen. */
    crazypod_miniapps_feature_reset_input();
    (void)crazypod_miniapps_event(&native_event);
    (void)crazypod_miniapps_take_ui_refresh();
    return true;
}

void crazypod_now_playing_theme_close(void)
{
    if(!crazypod_now_playing_theme_open())
        return;
    crazypod_miniapps_feature_reset_input();
    crazypod_miniapps_close();
}

#endif
