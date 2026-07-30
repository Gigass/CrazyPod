#ifndef CRAZYPOD_MINIAPP_SDK_H
#define CRAZYPOD_MINIAPP_SDK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CP_MINIAPP_ABI_VERSION 1u
#define CP_MINIAPP_SDK_REVISION 4u
#define CP_MINIAPP_BINARY_MAGIC 0x43504d41ul /* CPMA */
#define CP_MINIAPP_MAX_COMMANDS 64
#define CP_MINIAPP_TEXT_SIZE 48
#define CP_MINIAPP_TOAST_TEXT_SIZE 96
#define CP_MINIAPP_ALARM_SLOT_COUNT 4u
#define CP_MINIAPP_UI_TITLE_SIZE 48
#define CP_MINIAPP_UI_MESSAGE_SIZE 96
#define CP_MINIAPP_UI_VALUE_SIZE 256
#define CP_MINIAPP_UI_CHOICE_LABEL_SIZE 48
#define CP_MINIAPP_UI_CHOICE_MAX 32u
#define CP_MINIAPP_RESOURCE_ID_SIZE 32
#define CP_MINIAPP_MEDIA_TEXT_SIZE 64

enum cp_host_capability {
    CP_CAP_SYSTEM_INFO = 1u << 0,
    CP_CAP_FORMAT_DURATION = 1u << 1,
    CP_CAP_FORMAT_DATETIME = 1u << 2,
    CP_CAP_MULTIPLE_ALARMS = 1u << 3,
    CP_CAP_UI_TOAST = 1u << 4,
    CP_CAP_UI_REQUEST_CLOSE = 1u << 5,
    CP_CAP_DRAW_DIVIDER = 1u << 6,
    CP_CAP_DRAW_PROGRESS = 1u << 7,
    CP_CAP_UI_MODAL = 1u << 8,
    CP_CAP_RESOURCES = 1u << 9,
    CP_CAP_DRAW_BITMAP = 1u << 10,
    CP_CAP_NOW_PLAYING = 1u << 11
};

enum cp_system_flag {
    CP_SYSTEM_TIME_VALID = 1u << 0,
    CP_SYSTEM_EXTERNAL_POWER = 1u << 1,
    CP_SYSTEM_CHARGING = 1u << 2,
    CP_SYSTEM_USB_CONNECTED = 1u << 3,
    CP_SYSTEM_AUDIO_PLAYING = 1u << 4,
    CP_SYSTEM_AUDIO_PAUSED = 1u << 5,
    CP_SYSTEM_REDUCE_MOTION = 1u << 6
};

enum cp_language {
    CP_LANGUAGE_ENGLISH = 0,
    CP_LANGUAGE_CHINESE_SIMPLIFIED,
    CP_LANGUAGE_CHINESE_TRADITIONAL,
    CP_LANGUAGE_JAPANESE,
    CP_LANGUAGE_KOREAN,
    CP_LANGUAGE_GERMAN,
    CP_LANGUAGE_FRENCH,
    CP_LANGUAGE_SPANISH,
    CP_LANGUAGE_PORTUGUESE_BRAZIL,
    CP_LANGUAGE_COUNT
};

enum cp_datetime_format {
    CP_DATETIME_DATE = 0,
    CP_DATETIME_TIME,
    CP_DATETIME_DATE_TIME
};

struct cp_system_info {
    uint32_t struct_size;
    uint32_t flags;
    int16_t battery_percent;
    int16_t battery_minutes;
    uint32_t language;
};

enum cp_ui_result_status {
    CP_UI_RESULT_ACCEPTED = 1,
    CP_UI_RESULT_CANCELLED = 2
};

struct cp_ui_choice_item {
    char label[CP_MINIAPP_UI_CHOICE_LABEL_SIZE];
};

struct cp_ui_result {
    uint32_t struct_size;
    uint32_t request_id;
    int32_t status;
    int32_t selected_index;
    char value[CP_MINIAPP_UI_VALUE_SIZE];
};

enum cp_resource_type {
    CP_RESOURCE_BLOB = 0,
    CP_RESOURCE_BITMAP_RGB565 = 1
};

struct cp_resource_info {
    uint32_t struct_size;
    uint32_t size;
    uint32_t crc32;
    uint16_t width;
    uint16_t height;
    uint8_t type;
    uint8_t reserved[3];
};

enum cp_now_playing_flag {
    CP_NOW_PLAYING_AVAILABLE = 1u << 0,
    CP_NOW_PLAYING_PLAYING = 1u << 1,
    CP_NOW_PLAYING_PAUSED = 1u << 2
};

struct cp_now_playing {
    uint32_t struct_size;
    uint32_t flags;
    uint32_t elapsed_ms;
    uint32_t duration_ms;
    char title[CP_MINIAPP_MEDIA_TEXT_SIZE];
    char artist[CP_MINIAPP_MEDIA_TEXT_SIZE];
    char album[CP_MINIAPP_MEDIA_TEXT_SIZE];
};

enum cp_color_token {
    CP_COLOR_BACKGROUND = 0,
    CP_COLOR_SURFACE,
    CP_COLOR_SURFACE_RAISED,
    CP_COLOR_WHITE,
    CP_COLOR_MUTED,
    CP_COLOR_ACCENT,
    CP_COLOR_ACCENT_FOREGROUND,
    CP_COLOR_ROSE,
    CP_COLOR_GREEN,
    CP_COLOR_CYAN,
    CP_COLOR_AMBER,
    CP_COLOR_ERROR,
    CP_COLOR_COUNT
};

enum cp_font_token {
    CP_FONT_CAPTION = 0,
    CP_FONT_LABEL,
    CP_FONT_BODY,
    CP_FONT_CJK,
    CP_FONT_TITLE,
    CP_FONT_NUMBER,
    CP_FONT_DISPLAY,
    CP_FONT_COUNT
};

enum cp_text_align {
    CP_ALIGN_LEFT = 0,
    CP_ALIGN_CENTER,
    CP_ALIGN_RIGHT
};

enum cp_draw_type {
    CP_DRAW_RECT = 0,
    CP_DRAW_TEXT,
    CP_DRAW_RING,
    CP_DRAW_DIVIDER,
    CP_DRAW_PROGRESS,
    /* text contains a CP_RESOURCE_BITMAP_RGB565 resource id. */
    CP_DRAW_BITMAP
};

enum cp_draw_flags {
    CP_DRAW_FOCUSED = 1u << 0,
    CP_DRAW_CIRCLE = 1u << 1
};

struct cp_draw_command {
    uint8_t type;
    uint8_t flags;
    uint8_t font;
    uint8_t align;
    uint8_t foreground;
    uint8_t background;
    uint8_t border;
    uint8_t opacity;
    uint8_t border_opacity;
    uint8_t border_width;
    uint8_t radius;
    uint8_t track_color;
    uint8_t progress_color;
    uint8_t track_width;
    uint8_t progress_width;
    uint8_t reserved;
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    int32_t value;
    int32_t maximum;
    char text[CP_MINIAPP_TEXT_SIZE];
};

struct cp_scene {
    uint32_t struct_size;
    uint32_t background;
    uint16_t command_count;
    uint16_t reserved;
    struct cp_draw_command commands[CP_MINIAPP_MAX_COMMANDS];
};

enum cp_input_type {
    CP_INPUT_WHEEL_CLOCKWISE = 0,
    CP_INPUT_WHEEL_COUNTERCLOCKWISE,
    CP_INPUT_LEFT,
    CP_INPUT_RIGHT,
    CP_INPUT_SELECT,
    CP_INPUT_PLAY,
    CP_INPUT_MENU
};

struct cp_input_event {
    uint32_t struct_size;
    uint8_t type;
    /* Acceleration magnitude for continuous values; discrete focus moves
       exactly one item per wheel event. */
    uint8_t steps;
    uint8_t repeated;
    uint8_t reserved;
};

struct cp_host_api {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t (*epoch_seconds)(void);
    uint32_t (*monotonic_ms)(void);
    int (*state_read)(void *buffer, size_t capacity);
    int (*state_write)(const void *buffer, size_t size);
    int (*alarm_set)(uint32_t deadline_epoch, uint32_t token);
    void (*alarm_cancel)(void);
    bool (*alarm_fired)(uint32_t *token);
    void (*alarm_acknowledge)(void);
    void (*format_number)(double value, char *buffer, size_t capacity);
    /*
     * ABI 1 revision 2 tail. The fields above this point are the immutable
     * ABI 1 prefix. Apps must check struct_size and capabilities before
     * calling any field below.
     */
    uint32_t capabilities;
    int (*system_info)(struct cp_system_info *info);
    void (*format_duration)(uint32_t seconds, char *buffer,
                            size_t capacity);
    void (*format_datetime)(uint32_t epoch_seconds,
                            enum cp_datetime_format format,
                            char *buffer, size_t capacity);
    int (*alarm_set_slot)(uint8_t slot, uint32_t deadline_epoch,
                          uint32_t token);
    void (*alarm_cancel_slot)(uint8_t slot);
    bool (*alarm_fired_slot)(uint8_t slot, uint32_t *token);
    void (*alarm_acknowledge_slot)(uint8_t slot);
    int (*ui_toast)(const char *text, uint32_t duration_ms);
    int (*ui_request_close)(void);
    /*
     * ABI 1 revision 3 tail. Requests are host-owned and asynchronous.
     * The host copies all request data before returning.
     */
    int (*ui_text_input)(uint32_t request_id, const char *title,
                         const char *initial_value, uint16_t max_bytes);
    int (*ui_choice)(uint32_t request_id, const char *title,
                     const struct cp_ui_choice_item *items,
                     uint16_t item_count, int16_t selected_index);
    int (*ui_confirm)(uint32_t request_id, const char *title,
                      const char *message, const char *confirm_label);
    int (*ui_poll_result)(struct cp_ui_result *result);
    int (*ui_cancel)(uint32_t request_id);
    int (*resource_stat)(const char *id,
                         struct cp_resource_info *info);
    int (*resource_read)(const char *id, uint32_t offset,
                         void *buffer, size_t capacity);
    int (*now_playing)(struct cp_now_playing *info);
};

#define CP_HOST_API_V1_SIZE \
    offsetof(struct cp_host_api, capabilities)

#define CP_HOST_HAS(host, capability, field)                             \
    ((host) != NULL &&                                                   \
     (host)->struct_size >=                                              \
         offsetof(struct cp_host_api, field) +                           \
             sizeof((host)->field) &&                                    \
     (((host)->capabilities & (capability)) != 0) &&                     \
     (host)->field != NULL)

struct cp_miniapp_ops {
    uint32_t abi_version;
    uint32_t struct_size;
    const char *id;
    const char *name;
    const char *version;
    void (*open)(void);
    void (*close)(void);
    bool (*event)(const struct cp_input_event *event);
    bool (*tick)(uint32_t epoch_seconds, uint32_t monotonic_ms);
    void (*render)(struct cp_scene *scene);
};

typedef const struct cp_miniapp_ops *
    (*cp_miniapp_entry_fn)(const struct cp_host_api *host);

static inline void cp_scene_reset(struct cp_scene *scene)
{
    size_t i;

    if(scene == NULL)
        return;
    scene->struct_size = sizeof(*scene);
    scene->background = CP_COLOR_BACKGROUND;
    scene->command_count = 0;
    scene->reserved = 0;
    for(i = 0; i < sizeof(scene->commands); ++i)
        ((uint8_t *)scene->commands)[i] = 0;
}

static inline void cp_text_copy(char *destination, size_t capacity,
                                const char *source)
{
    size_t index = 0;

    if(destination == NULL || capacity == 0)
        return;
    if(source != NULL) {
        while(index + 1 < capacity && source[index] != '\0') {
            destination[index] = source[index];
            ++index;
        }
    }
    destination[index] = '\0';
}

static inline struct cp_draw_command *
cp_scene_add(struct cp_scene *scene, enum cp_draw_type type)
{
    struct cp_draw_command *command;
    size_t index;

    if(scene == NULL ||
       scene->command_count >= CP_MINIAPP_MAX_COMMANDS)
        return NULL;
    command = &scene->commands[scene->command_count++];
    for(index = 0; index < sizeof(*command); ++index)
        ((uint8_t *)command)[index] = 0;
    command->type = (uint8_t)type;
    command->foreground = CP_COLOR_WHITE;
    command->background = CP_COLOR_SURFACE;
    command->border = CP_COLOR_WHITE;
    command->opacity = 255;
    command->maximum = 100;
    return command;
}

#ifdef CRAZYPOD_MINIAPP_PACKAGE
#include "config.h"
#include "load_code.h"

struct cp_miniapp_binary_header {
    struct lc_header lc_header;
    cp_miniapp_entry_fn entry;
    unsigned char *bss_start;
    uint32_t host_api_size;
    uint32_t ops_size;
};

extern const struct cp_miniapp_ops *
cp_miniapp_entry(const struct cp_host_api *host);

#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
extern unsigned char plugin_start_addr[];
extern unsigned char plugin_end_addr[];
extern unsigned char plugin_bss_start[];
#define CP_MINIAPP_HEADER                                                \
    const struct cp_miniapp_binary_header __header                      \
        __attribute__((section(".header"))) = {                         \
            { CP_MINIAPP_BINARY_MAGIC, TARGET_ID,                       \
              CP_MINIAPP_ABI_VERSION, plugin_start_addr,                \
              plugin_end_addr },                                       \
            cp_miniapp_entry, plugin_bss_start,                         \
            sizeof(struct cp_host_api),                                 \
            sizeof(struct cp_miniapp_ops)                               \
        }
#else
#define CP_MINIAPP_HEADER                                                \
    const struct cp_miniapp_binary_header __header                      \
        __attribute__((visibility("default"))) = {                      \
            { CP_MINIAPP_BINARY_MAGIC, TARGET_ID,                       \
              CP_MINIAPP_ABI_VERSION, NULL, NULL },                     \
            cp_miniapp_entry, NULL,                                     \
            sizeof(struct cp_host_api),                                 \
            sizeof(struct cp_miniapp_ops)                               \
        }
#endif
#endif

#endif
