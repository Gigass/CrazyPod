#include <stdio.h>
#include <string.h>

#include "../../crazypod_miniapps.h"
#include "../installer/crazypod_miniapp_manifest.h"
#include "crazypod_miniapp_modal.h"

enum modal_type {
    MODAL_NONE,
    MODAL_TEXT,
    MODAL_CHOICE,
    MODAL_CONFIRM,
};

static struct {
    enum modal_type type;
    bool session_open;
    bool result_ready;
    bool changed;
    uint32_t request_id;
    uint16_t max_bytes;
    uint16_t item_count;
    int16_t selected;
    uint8_t character;
    char title[CP_MINIAPP_UI_TITLE_SIZE];
    char message[CP_MINIAPP_UI_MESSAGE_SIZE];
    char confirm_label[CP_MINIAPP_UI_CHOICE_LABEL_SIZE];
    char value[CP_MINIAPP_UI_VALUE_SIZE];
    struct cp_ui_choice_item items[CP_MINIAPP_UI_CHOICE_MAX];
    struct cp_ui_result result;
} modal;

static size_t bounded_length(const char *text, size_t capacity)
{
    size_t length = 0;

    if(text == NULL)
        return capacity;
    while(length < capacity && text[length] != '\0')
        ++length;
    return length;
}

static bool copy_text(
    char *destination, size_t capacity,
    const char *source, size_t length)
{
    if(length >= capacity)
        return false;
    memcpy(destination, source, length);
    destination[length] = '\0';
    return true;
}

static bool request_available(uint32_t request_id)
{
    return modal.session_open && request_id != 0 &&
           modal.type == MODAL_NONE && !modal.result_ready;
}

void crazypod_miniapp_modal_open(void)
{
    memset(&modal, 0, sizeof(modal));
    modal.session_open = true;
}

void crazypod_miniapp_modal_close(void)
{
    memset(&modal, 0, sizeof(modal));
}

int crazypod_miniapp_modal_text_input(
    uint32_t request_id, const char *title,
    const char *initial_value, uint16_t max_bytes)
{
    size_t title_length;
    size_t value_length;
    size_t index;

    if(!request_available(request_id) ||
       title == NULL || initial_value == NULL)
        return CRAZYPOD_MINIAPP_ERROR_BUSY;
    title_length = bounded_length(title, sizeof(modal.title));
    value_length = bounded_length(initial_value, sizeof(modal.value));
    if(title_length == 0 || title_length >= sizeof(modal.title) ||
       value_length >= sizeof(modal.value) || max_bytes == 0 ||
       max_bytes >= sizeof(modal.value) || value_length > max_bytes)
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    if(!crazypod_miniapp_text_valid(title, true))
        return CRAZYPOD_MINIAPP_ERROR_UNSUPPORTED;
    for(index = 0; index < value_length; ++index) {
        unsigned char value = (unsigned char)initial_value[index];
        if(value < 0x20 || value > 0x7e)
            return CRAZYPOD_MINIAPP_ERROR_UNSUPPORTED;
    }
    modal.type = MODAL_TEXT;
    modal.request_id = request_id;
    modal.max_bytes = max_bytes;
    copy_text(modal.title, sizeof(modal.title), title, title_length);
    copy_text(modal.value, sizeof(modal.value),
              initial_value, value_length);
    modal.changed = true;
    return CRAZYPOD_MINIAPP_OK;
}

int crazypod_miniapp_modal_choice(
    uint32_t request_id, const char *title,
    const struct cp_ui_choice_item *items,
    uint16_t item_count, int16_t selected_index)
{
    size_t title_length;
    uint16_t index;

    if(!request_available(request_id) || title == NULL || items == NULL)
        return CRAZYPOD_MINIAPP_ERROR_BUSY;
    title_length = bounded_length(title, sizeof(modal.title));
    if(title_length == 0 || title_length >= sizeof(modal.title) ||
       item_count == 0 || item_count > CP_MINIAPP_UI_CHOICE_MAX ||
       selected_index < 0 || selected_index >= (int16_t)item_count)
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    if(!crazypod_miniapp_text_valid(title, true))
        return CRAZYPOD_MINIAPP_ERROR_UNSUPPORTED;
    for(index = 0; index < item_count; ++index) {
        size_t length = bounded_length(
            items[index].label, sizeof(items[index].label));
        if(length == 0 || length >= sizeof(items[index].label))
            return CRAZYPOD_MINIAPP_ERROR_LIMIT;
        if(!crazypod_miniapp_text_valid(items[index].label, true))
            return CRAZYPOD_MINIAPP_ERROR_UNSUPPORTED;
    }
    modal.type = MODAL_CHOICE;
    modal.request_id = request_id;
    modal.item_count = item_count;
    modal.selected = selected_index;
    copy_text(modal.title, sizeof(modal.title), title, title_length);
    memcpy(modal.items, items, (size_t)item_count * sizeof(items[0]));
    modal.changed = true;
    return CRAZYPOD_MINIAPP_OK;
}

int crazypod_miniapp_modal_confirm(
    uint32_t request_id, const char *title,
    const char *message, const char *confirm_label)
{
    size_t title_length;
    size_t message_length;
    size_t label_length;

    if(!request_available(request_id) || title == NULL ||
       message == NULL || confirm_label == NULL)
        return CRAZYPOD_MINIAPP_ERROR_BUSY;
    title_length = bounded_length(title, sizeof(modal.title));
    message_length = bounded_length(message, sizeof(modal.message));
    label_length = bounded_length(
        confirm_label, sizeof(modal.confirm_label));
    if(title_length == 0 || title_length >= sizeof(modal.title) ||
       message_length == 0 || message_length >= sizeof(modal.message) ||
       label_length == 0 || label_length >= sizeof(modal.confirm_label))
        return CRAZYPOD_MINIAPP_ERROR_LIMIT;
    if(!crazypod_miniapp_text_valid(title, true) ||
       !crazypod_miniapp_text_valid(message, true) ||
       !crazypod_miniapp_text_valid(confirm_label, true))
        return CRAZYPOD_MINIAPP_ERROR_UNSUPPORTED;
    modal.type = MODAL_CONFIRM;
    modal.request_id = request_id;
    modal.selected = 0;
    copy_text(modal.title, sizeof(modal.title), title, title_length);
    copy_text(modal.message, sizeof(modal.message), message, message_length);
    copy_text(modal.confirm_label, sizeof(modal.confirm_label),
              confirm_label, label_length);
    modal.changed = true;
    return CRAZYPOD_MINIAPP_OK;
}

static void finish(int status)
{
    memset(&modal.result, 0, sizeof(modal.result));
    modal.result.struct_size = sizeof(modal.result);
    modal.result.request_id = modal.request_id;
    modal.result.status = status;
    modal.result.selected_index =
        modal.type == MODAL_CHOICE || modal.type == MODAL_CONFIRM
            ? modal.selected : -1;
    if(modal.type == MODAL_TEXT)
        cp_text_copy(modal.result.value, sizeof(modal.result.value),
                     modal.value);
    modal.result_ready = true;
    modal.type = MODAL_NONE;
    modal.changed = true;
}

int crazypod_miniapp_modal_poll(struct cp_ui_result *result)
{
    if(result == NULL || result->struct_size < sizeof(*result))
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    if(!modal.result_ready)
        return 0;
    *result = modal.result;
    memset(&modal.result, 0, sizeof(modal.result));
    modal.result_ready = false;
    return 1;
}

int crazypod_miniapp_modal_cancel(uint32_t request_id)
{
    if(modal.type == MODAL_NONE || modal.request_id != request_id)
        return CRAZYPOD_MINIAPP_ERROR_STATE;
    finish(CP_UI_RESULT_CANCELLED);
    return CRAZYPOD_MINIAPP_OK;
}

bool crazypod_miniapp_modal_event(const struct cp_input_event *event)
{
    static const char characters[] =
        " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,:+-*/()=%!?_";
    int direction = 0;

    if(modal.type == MODAL_NONE)
        return false;
    if(event->type == CP_INPUT_WHEEL_CLOCKWISE)
        direction = 1;
    else if(event->type == CP_INPUT_WHEEL_COUNTERCLOCKWISE)
        direction = -1;
    if(modal.type == MODAL_TEXT) {
        size_t length = strlen(modal.value);
        if(direction != 0) {
            int count = (int)sizeof(characters) - 1;
            int next = (int)modal.character + direction;
            modal.character = (uint8_t)(
                next < 0 ? count - 1 : next >= count ? 0 : next);
        }
        else if(event->type == CP_INPUT_SELECT &&
                length < modal.max_bytes &&
                length + 1 < sizeof(modal.value)) {
            modal.value[length] = characters[modal.character];
            modal.value[length + 1] = '\0';
        }
        else if((event->type == CP_INPUT_LEFT ||
                 event->type == CP_INPUT_PLAY) && length > 0)
            modal.value[length - 1] = '\0';
        else if(event->type == CP_INPUT_RIGHT)
            finish(CP_UI_RESULT_ACCEPTED);
        else if(event->type == CP_INPUT_MENU)
            finish(CP_UI_RESULT_CANCELLED);
    }
    else if(modal.type == MODAL_CHOICE) {
        if(direction != 0) {
            int next = modal.selected + direction;
            if(next < 0) next = 0;
            if(next >= modal.item_count) next = modal.item_count - 1;
            modal.selected = (int16_t)next;
        }
        else if(event->type == CP_INPUT_SELECT ||
                event->type == CP_INPUT_RIGHT)
            finish(CP_UI_RESULT_ACCEPTED);
        else if(event->type == CP_INPUT_MENU ||
                event->type == CP_INPUT_LEFT)
            finish(CP_UI_RESULT_CANCELLED);
    }
    else {
        if(direction != 0 || event->type == CP_INPUT_LEFT ||
           event->type == CP_INPUT_RIGHT)
            modal.selected = modal.selected == 0 ? 1 : 0;
        else if(event->type == CP_INPUT_SELECT)
            finish(modal.selected == 1
                ? CP_UI_RESULT_ACCEPTED : CP_UI_RESULT_CANCELLED);
        else if(event->type == CP_INPUT_MENU)
            finish(CP_UI_RESULT_CANCELLED);
    }
    modal.changed = true;
    return true;
}

static struct cp_draw_command *command(
    struct cp_scene *scene, enum cp_draw_type type,
    int x, int y, int width, int height)
{
    struct cp_draw_command *draw = cp_scene_add(scene, type);
    if(draw != NULL) {
        draw->x = x; draw->y = y;
        draw->width = width; draw->height = height;
    }
    return draw;
}

static void text(
    struct cp_scene *scene, const char *value,
    int x, int y, int width, enum cp_font_token font,
    enum cp_text_align align, bool focused)
{
    struct cp_draw_command *draw =
        command(scene, CP_DRAW_TEXT, x, y, width, 20);
    if(draw == NULL)
        return;
    draw->font = font;
    draw->align = align;
    draw->foreground =
        focused ? CP_COLOR_ACCENT_FOREGROUND : CP_COLOR_WHITE;
    if(focused)
        draw->flags |= CP_DRAW_FOCUSED;
    cp_text_copy(draw->text, sizeof(draw->text), value);
}

static void wrapped_text(
    struct cp_scene *scene, const char *value,
    int x, int y, int width)
{
    const char *cursor = value;
    int line;

    for(line = 0; line < 3 && *cursor != '\0'; ++line) {
        char part[CP_MINIAPP_TEXT_SIZE];
        size_t remaining = strlen(cursor);
        size_t amount = remaining < sizeof(part) - 1
            ? remaining : sizeof(part) - 1;
        while(amount > 0 && remaining > amount &&
              (((unsigned char)cursor[amount] & 0xc0) == 0x80))
            --amount;
        if(amount == 0)
            return;
        memcpy(part, cursor, amount);
        part[amount] = '\0';
        text(scene, part, x, y + line * 22, width,
             CP_FONT_BODY, CP_ALIGN_CENTER, false);
        cursor += amount;
    }
}

void crazypod_miniapp_modal_render(struct cp_scene *scene)
{
    struct cp_draw_command *panel;

    if(modal.type == MODAL_NONE)
        return;
    if(scene->command_count > CP_MINIAPP_MAX_COMMANDS - 14)
        scene->command_count = CP_MINIAPP_MAX_COMMANDS - 14;
    panel = command(scene, CP_DRAW_RECT, 16, 34, 288, 194);
    if(panel == NULL)
        return;
    panel->background = CP_COLOR_SURFACE_RAISED;
    panel->border = CP_COLOR_WHITE;
    panel->border_width = 1;
    panel->border_opacity = 80;
    panel->radius = 14;
    text(scene, modal.title, 30, 48, 260,
         CP_FONT_TITLE, CP_ALIGN_LEFT, false);
    if(modal.type == MODAL_TEXT) {
        char character[4] = {
            '[', " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
                 ".,:+-*/()=%!?_"[modal.character], ']', '\0'
        };
        char counter[24];
        struct cp_draw_command *field =
            command(scene, CP_DRAW_RECT, 28, 80, 264, 52);
        if(field != NULL) {
            field->background = CP_COLOR_SURFACE;
            field->radius = 8;
        }
        text(scene, modal.value, 38, 96, 244,
             CP_FONT_BODY, CP_ALIGN_LEFT, false);
        text(scene, character, 112, 143, 96,
             CP_FONT_DISPLAY, CP_ALIGN_CENTER, true);
        snprintf(counter, sizeof(counter), "%lu/%u",
                 (unsigned long)strlen(modal.value),
                 (unsigned)modal.max_bytes);
        text(scene, counter, 220, 151, 58,
             CP_FONT_CAPTION, CP_ALIGN_RIGHT, false);
        text(scene, "SELECT ADD  PLAY DELETE", 34, 184, 252,
             CP_FONT_CAPTION, CP_ALIGN_CENTER, false);
        text(scene, "RIGHT DONE  MENU CANCEL", 34, 204, 252,
             CP_FONT_CAPTION, CP_ALIGN_CENTER, false);
    }
    else if(modal.type == MODAL_CHOICE) {
        int start = modal.selected - 2;
        int row;
        if(start < 0) start = 0;
        if(start > modal.item_count - 5)
            start = modal.item_count > 5 ? modal.item_count - 5 : 0;
        for(row = 0; row < 5 && start + row < modal.item_count; ++row) {
            int item = start + row;
            bool focused = item == modal.selected;
            struct cp_draw_command *background =
                command(scene, CP_DRAW_RECT, 28, 78 + row * 27, 264, 23);
            if(background != NULL) {
                background->background =
                    focused ? CP_COLOR_ACCENT : CP_COLOR_SURFACE;
                background->opacity = focused ? 255 : 160;
                background->radius = 7;
            }
            text(scene, modal.items[item].label, 38, 81 + row * 27, 244,
                 CP_FONT_LABEL, CP_ALIGN_LEFT, focused);
        }
    }
    else {
        struct cp_draw_command *cancel =
            command(scene, CP_DRAW_RECT, 34, 164, 116, 40);
        struct cp_draw_command *confirm =
            command(scene, CP_DRAW_RECT, 170, 164, 116, 40);
        wrapped_text(scene, modal.message, 34, 84, 252);
        if(cancel != NULL) {
            cancel->background =
                modal.selected == 0 ? CP_COLOR_ACCENT : CP_COLOR_SURFACE;
            cancel->radius = 8;
        }
        if(confirm != NULL) {
            confirm->background =
                modal.selected == 1 ? CP_COLOR_ACCENT : CP_COLOR_SURFACE;
            confirm->radius = 8;
        }
        text(scene, "Cancel", 34, 176, 116,
             CP_FONT_LABEL, CP_ALIGN_CENTER, modal.selected == 0);
        text(scene, modal.confirm_label, 170, 176, 116,
             CP_FONT_LABEL, CP_ALIGN_CENTER, modal.selected == 1);
    }
}

bool crazypod_miniapp_modal_take_changed(void)
{
    bool changed = modal.changed;
    modal.changed = false;
    return changed;
}
