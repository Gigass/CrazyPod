#ifndef CRAZYPOD_MINIAPP_NATIVE_SDK_H
#define CRAZYPOD_MINIAPP_NATIVE_SDK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * CrazyPod Native Miniapp ABI 1.
 *
 * This header is the only binary contract shared by generated miniapp C and
 * the firmware.  It deliberately exposes handles and plain data only.  LVGL
 * objects, Rockbox internals and compiler-runtime state remain host-owned.
 */
#define CP_NATIVE_ABI_MAJOR 1u
#define CP_NATIVE_ABI_MINOR 3u
#define CP_NATIVE_PACKAGE_FORMAT 5u
#define CP_NATIVE_REACT_PROFILE 1u
#define CP_NATIVE_BINARY_MAGIC 0x35414d43ul /* CMA5 */

#define CP_NATIVE_UI_HANDLE_NONE 0u
#define CP_NATIVE_UI_EVENT_NONE 0u
#define CP_NATIVE_UI_HANDLE_MAX 512u
#define CP_NATIVE_UI_HANDLE_INDEX_BITS 9u
#define CP_NATIVE_UI_HANDLE_INDEX_MASK 0x1ffu
#define CP_NATIVE_UI_HANDLE_GENERATION_SHIFT \
    CP_NATIVE_UI_HANDLE_INDEX_BITS
#define CP_NATIVE_RESOURCE_ID_SIZE 64u
#define CP_NATIVE_TEXT_MAX 256u
#define CP_NATIVE_SESSION_MAX (12u * 1024u * 1024u)
#define CP_NATIVE_MANIFEST_MAX (8u * 1024u)
#define CP_NATIVE_ASSET_MAX (8u * 1024u * 1024u)
#define CP_NATIVE_UI_PAYLOAD_MAX (64u * 1024u)
#define CP_NATIVE_SERVICE_PAYLOAD_MAX 1024u

#define CP_NATIVE_SERVICE_SYSTEM 0u
#define CP_NATIVE_SYSTEM_INFO 0u

#define CP_UI_HANDLE_MAX CP_NATIVE_UI_HANDLE_MAX
#define CP_UI_HANDLE_NONE CP_NATIVE_UI_HANDLE_NONE
#define CP_UI_EVENT_NONE CP_NATIVE_UI_EVENT_NONE
#define CP_UI_HANDLE_INDEX(handle) \
    ((uint32_t)(handle) & CP_NATIVE_UI_HANDLE_INDEX_MASK)
#define CP_UI_HANDLE_GENERATION(handle) \
    ((uint32_t)(handle) >> \
     CP_NATIVE_UI_HANDLE_GENERATION_SHIFT)
#define CP_UI_HANDLE_MAKE(index, generation) \
    (((uint32_t)(generation) << \
      CP_NATIVE_UI_HANDLE_GENERATION_SHIFT) | \
     (uint32_t)(index))

#define CP_CANVAS_MAGIC 0x35464343u /* CCF5 */
#define CP_CANVAS_COMMAND_ABI 1u
#define CP_CANVAS_HEADER_SIZE 24u
#define CP_CANVAS_COMMAND_SIZE 24u
#define CP_CANVAS_COMMAND_MAX 2048u
#define CP_TILEMAP_MAGIC 0x35545043u /* CPT5 */
#define CP_TILEMAP_ABI 1u
#define CP_TILEMAP_HEADER_SIZE 48u
#define CP_TILEMAP_SPRITE_SIZE 8u
#define CP_TILEMAP_WIDTH_MAX 64u
#define CP_TILEMAP_HEIGHT_MAX 64u
#define CP_TILEMAP_LAYER_MAX 2u
#define CP_TILEMAP_SPRITE_MAX 32u
#define CP_CHART_DATA_MAGIC 0x35544843u /* CHT5 */
#define CP_CHART_POINT_MAX 256u
#define CP_TABLE_DATA_MAGIC 0x354c4254u /* TBL5 */
#define CP_TABLE_ROW_MAX 32u
#define CP_TABLE_COLUMN_MAX 8u
#define CP_TABLE_CELL_MAX 63u

typedef uint32_t cp_ui_handle_t;
typedef uint32_t cp_event_handler_t;

enum cp_native_result {
    CP_NATIVE_OK = 0,
    CP_NATIVE_ERROR_ARGUMENT = -1,
    CP_NATIVE_ERROR_STATE = -2,
    CP_NATIVE_ERROR_LIMIT = -3,
    CP_NATIVE_ERROR_UNSUPPORTED = -4,
    CP_NATIVE_ERROR_IO = -5
};

enum cp_native_update_result {
    CP_NATIVE_UPDATE_NONE = 0,
    CP_NATIVE_UPDATE_UI = 1u << 0,
    CP_NATIVE_UPDATE_CLOSE = 1u << 1,
    CP_NATIVE_UPDATE_SCHEDULED = 1u << 2
};

enum cp_ui_object_type {
    CP_UI_OBJECT_SCREEN = 1,
    CP_UI_OBJECT_VIEW,
    CP_UI_OBJECT_TEXT,
    CP_UI_OBJECT_IMAGE,
    CP_UI_OBJECT_BUTTON,
    CP_UI_OBJECT_SCROLL_VIEW,
    CP_UI_OBJECT_LIST,
    CP_UI_OBJECT_PROGRESS,
    CP_UI_OBJECT_ARC,
    CP_UI_OBJECT_SLIDER,
    CP_UI_OBJECT_SWITCH,
    CP_UI_OBJECT_TEXT_INPUT,
    CP_UI_OBJECT_CANVAS,
    CP_UI_OBJECT_TILEMAP,
    CP_UI_OBJECT_ANIMATED_IMAGE,
    CP_UI_OBJECT_CHART,
    CP_UI_OBJECT_CHECKBOX,
    CP_UI_OBJECT_DROPDOWN,
    CP_UI_OBJECT_ROLLER,
    CP_UI_OBJECT_TABLE,
    CP_UI_OBJECT_TILE_VIEW,
    CP_UI_OBJECT_IMAGE_BUTTON,
    CP_UI_OBJECT_TYPE_COUNT
};

enum cp_ui_property {
    CP_UI_PROP_VISIBLE = 1,
    CP_UI_PROP_DISABLED,
    CP_UI_PROP_FOCUSED,
    CP_UI_PROP_FOCUSABLE,
    CP_UI_PROP_X,
    CP_UI_PROP_Y,
    CP_UI_PROP_WIDTH,
    CP_UI_PROP_HEIGHT,
    CP_UI_PROP_MIN_WIDTH,
    CP_UI_PROP_MIN_HEIGHT,
    CP_UI_PROP_MAX_WIDTH,
    CP_UI_PROP_MAX_HEIGHT,
    CP_UI_PROP_LAYOUT,
    CP_UI_PROP_FLEX_FLOW,
    CP_UI_PROP_FLEX_GROW,
    CP_UI_PROP_GRID_COLUMNS,
    CP_UI_PROP_GRID_ROWS,
    CP_UI_PROP_ALIGN,
    CP_UI_PROP_JUSTIFY,
    CP_UI_PROP_PADDING,
    CP_UI_PROP_PADDING_LEFT,
    CP_UI_PROP_PADDING_RIGHT,
    CP_UI_PROP_PADDING_TOP,
    CP_UI_PROP_PADDING_BOTTOM,
    CP_UI_PROP_MARGIN,
    CP_UI_PROP_BACKGROUND_COLOR,
    CP_UI_PROP_BACKGROUND_OPACITY,
    CP_UI_PROP_BORDER_COLOR,
    CP_UI_PROP_BORDER_WIDTH,
    CP_UI_PROP_BORDER_OPACITY,
    CP_UI_PROP_RADIUS,
    CP_UI_PROP_OPACITY,
    CP_UI_PROP_SHADOW_COLOR,
    CP_UI_PROP_SHADOW_WIDTH,
    CP_UI_PROP_SHADOW_OPACITY,
    CP_UI_PROP_TEXT,
    CP_UI_PROP_TEXT_COLOR,
    CP_UI_PROP_TEXT_ALIGN,
    CP_UI_PROP_FONT,
    CP_UI_PROP_IMAGE_SOURCE,
    CP_UI_PROP_VALUE,
    CP_UI_PROP_MINIMUM,
    CP_UI_PROP_MAXIMUM,
    CP_UI_PROP_CHECKED,
    CP_UI_PROP_PLACEHOLDER,
    CP_UI_PROP_SCROLL_X,
    CP_UI_PROP_SCROLL_Y,
    CP_UI_PROP_TRANSLATE_X,
    CP_UI_PROP_TRANSLATE_Y,
    CP_UI_PROP_SCALE_X,
    CP_UI_PROP_SCALE_Y,
    CP_UI_PROP_ROTATION,
    CP_UI_PROP_OVERFLOW,
    CP_UI_PROP_DATA,
    CP_UI_PROP_TILE_COLUMN,
    CP_UI_PROP_TILE_ROW,
    CP_UI_PROP_TILE_DIRECTIONS,
    CP_UI_PROP_SELECTED_COLUMN,
    CP_UI_PROP_SELECTED_ROW,
    CP_UI_PROP_MARGIN_LEFT,
    CP_UI_PROP_MARGIN_RIGHT,
    CP_UI_PROP_MARGIN_TOP,
    CP_UI_PROP_MARGIN_BOTTOM,
    CP_UI_PROP_POSITION,
    CP_UI_PROP_COUNT
};

enum cp_ui_position {
    CP_UI_POSITION_RELATIVE = 0,
    CP_UI_POSITION_ABSOLUTE
};

enum cp_ui_event_type {
    CP_UI_EVENT_SELECT = 1,
    CP_UI_EVENT_BACK,
    CP_UI_EVENT_FOCUS,
    CP_UI_EVENT_BLUR,
    CP_UI_EVENT_WHEEL,
    CP_UI_EVENT_KEY,
    CP_UI_EVENT_LONG_PRESS,
    CP_UI_EVENT_CHANGE,
    CP_UI_EVENT_SCROLL,
    CP_UI_EVENT_ANIMATION_COMPLETE
};

enum cp_ui_layout {
    CP_UI_LAYOUT_NONE = 0,
    CP_UI_LAYOUT_FLEX,
    CP_UI_LAYOUT_GRID
};

enum cp_ui_flex_flow {
    CP_UI_FLEX_ROW = 0,
    CP_UI_FLEX_COLUMN,
    CP_UI_FLEX_ROW_WRAP,
    CP_UI_FLEX_COLUMN_WRAP
};

enum cp_ui_align {
    CP_UI_PLACE_START = 0,
    CP_UI_PLACE_CENTER,
    CP_UI_PLACE_END,
    CP_UI_PLACE_SPACE_BETWEEN,
    CP_UI_PLACE_SPACE_AROUND,
    CP_UI_PLACE_SPACE_EVENLY
};

enum cp_ui_text_align {
    CP_UI_TEXT_ALIGN_LEFT = 0,
    CP_UI_TEXT_ALIGN_CENTER,
    CP_UI_TEXT_ALIGN_RIGHT
};

enum cp_ui_font {
    CP_UI_FONT_CAPTION = 0,
    CP_UI_FONT_LABEL,
    CP_UI_FONT_BODY,
    CP_UI_FONT_CJK,
    CP_UI_FONT_TITLE,
    CP_UI_FONT_NUMBER,
    CP_UI_FONT_DISPLAY,
    CP_UI_FONT_COUNT
};

enum cp_ui_animation_easing {
    CP_UI_EASING_LINEAR = 0,
    CP_UI_EASING_EASE_IN,
    CP_UI_EASING_EASE_OUT,
    CP_UI_EASING_EASE_IN_OUT,
    CP_UI_EASING_OVERSHOOT,
    CP_UI_EASING_BOUNCE
};

enum cp_canvas_opcode {
    CP_CANVAS_CLEAR = 1,
    CP_CANVAS_FILL_RECT,
    CP_CANVAS_LINE,
    CP_CANVAS_PIXEL,
    CP_CANVAS_CLIP,
    CP_CANVAS_RESET_CLIP,
    CP_CANVAS_TEXT,
    CP_CANVAS_IMAGE
};

struct cp_canvas_header {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t total_size;
    uint16_t command_count;
    uint16_t command_size;
    uint32_t payload_offset;
    uint32_t payload_size;
};

struct cp_canvas_command {
    uint8_t opcode;
    uint8_t opacity;
    uint16_t flags;
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    uint32_t color;
    uint32_t payload_offset;
    uint32_t payload_size;
};

struct cp_tilemap_header {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t total_size;
    uint16_t map_width;
    uint16_t map_height;
    uint8_t tile_width;
    uint8_t tile_height;
    uint8_t layer_count;
    uint8_t sprite_count;
    int16_t camera_x;
    int16_t camera_y;
    char tileset[24];
};

struct cp_tilemap_sprite {
    uint16_t tile;
    int16_t x;
    int16_t y;
    uint16_t flags;
};

struct cp_chart_data_header {
    uint32_t magic;
    uint16_t point_count;
    uint16_t reserved;
};

struct cp_table_data_header {
    uint32_t magic;
    uint32_t total_size;
    uint16_t row_count;
    uint16_t column_count;
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

enum cp_resource_type {
    CP_RESOURCE_BLOB = 0,
    CP_RESOURCE_BITMAP_RGB565,
    CP_RESOURCE_FONT,
    CP_RESOURCE_SPRITE_SHEET,
    CP_RESOURCE_TILESET,
    CP_RESOURCE_AUDIO_PCM
};

struct cp_input_event {
    uint32_t struct_size;
    uint8_t type;
    uint8_t steps;
    uint8_t repeated;
    uint8_t reserved;
};

struct cp_resource_info {
    uint32_t struct_size;
    uint32_t size;
    uint32_t crc32;
    uint16_t width;
    uint16_t height;
    uint8_t type;
    uint8_t frame_count;
    uint16_t frame_duration_ms;
};

struct cp_native_system_info {
    uint16_t abi_major;
    uint16_t abi_minor;
    uint32_t capabilities;
    uint32_t service_payload_max;
};

struct cp_native_ui_animation {
    int32_t from;
    int32_t to;
    uint32_t duration_ms;
    uint32_t delay_ms;
    uint16_t easing;
    uint16_t flags;
    cp_event_handler_t completion_handler;
};

/*
 * Every function returns CP_NATIVE_OK or a negative cp_native_result.
 * begin_update/end_update bracket one synchronous React commit.  The host
 * may defer LVGL layout until end_update but must copy string/byte arguments
 * before returning from their setter.
 */
struct cp_native_ui_api {
    uint16_t abi_major;
    uint16_t abi_minor;
    uint32_t struct_size;
    int (*begin_update)(void);
    cp_ui_handle_t (*create)(uint8_t object_type);
    int (*insert)(cp_ui_handle_t child, cp_ui_handle_t parent,
                  cp_ui_handle_t before);
    int (*set_i32)(cp_ui_handle_t target, uint16_t property,
                   int32_t value);
    int (*set_color)(cp_ui_handle_t target, uint16_t property,
                     uint32_t rgb);
    int (*set_string)(cp_ui_handle_t target, uint16_t property,
                      const char *value);
    int (*set_bytes)(cp_ui_handle_t target, uint16_t property,
                     const void *data, size_t size);
    int (*listen)(cp_ui_handle_t target, uint8_t event_type,
                  cp_event_handler_t handler);
    int (*animate)(cp_ui_handle_t target, uint16_t property,
                   const struct cp_native_ui_animation *animation);
    int (*commit_drawing)(cp_ui_handle_t target,
                          const void *data, size_t size);
    int (*remove)(cp_ui_handle_t target);
    int (*end_update)(void);
};

enum cp_native_host_capability {
    CP_NATIVE_CAP_STATE = 1u << 0,
    CP_NATIVE_CAP_RESOURCES = 1u << 1,
    CP_NATIVE_CAP_REQUEST_CLOSE = 1u << 2,
    CP_NATIVE_CAP_LOG = 1u << 3,
    CP_NATIVE_CAP_FILES = 1u << 4,
    CP_NATIVE_CAP_SERVICES = 1u << 5
};

struct cp_native_host_api {
    uint16_t abi_major;
    uint16_t abi_minor;
    uint32_t struct_size;
    uint32_t capabilities;
    const struct cp_native_ui_api *ui;
    uint32_t (*epoch_seconds)(void);
    uint32_t (*monotonic_ms)(void);
    int (*state_read)(void *buffer, size_t capacity);
    int (*state_write)(const void *buffer, size_t size);
    int (*resource_stat)(const char *id, struct cp_resource_info *info);
    int (*resource_read)(const char *id, uint32_t offset,
                         void *buffer, size_t capacity);
    int (*request_close)(void);
    void (*log)(uint8_t level, const char *message);
    int (*file_size)(const char *relative_path);
    int (*file_read)(const char *relative_path,
                     void *buffer, size_t capacity);
    int (*file_write)(const char *relative_path,
                      const void *buffer, size_t size);
    int (*file_remove)(const char *relative_path);
    int (*service_call)(uint32_t service, uint32_t operation,
                        const void *request, size_t request_size,
                        void *response, size_t response_capacity);
};

struct cp_native_app_ops {
    uint16_t abi_major;
    uint16_t abi_minor;
    uint32_t struct_size;
    const char *id;
    const char *name;
    const char *version;
    int (*mount)(void);
    void (*unmount)(void);
    uint32_t (*input)(const struct cp_input_event *event);
    uint32_t (*ui_event)(cp_event_handler_t handler,
                         uint8_t event_type,
                         cp_ui_handle_t target,
                         int32_t value);
    uint32_t (*tick)(uint32_t epoch_seconds, uint32_t monotonic_ms);
    bool (*has_scheduled_work)(void);
};

typedef const struct cp_native_app_ops *
    (*cp_native_entry_fn)(const struct cp_native_host_api *host);

#define CP_NATIVE_HOST_V1_SIZE sizeof(struct cp_native_host_api)
#define CP_NATIVE_UI_V1_SIZE sizeof(struct cp_native_ui_api)
#define CP_NATIVE_APP_OPS_V1_SIZE sizeof(struct cp_native_app_ops)

#ifdef CRAZYPOD_MINIAPP_PACKAGE
#ifdef CRAZYPOD_MINIAPP_STANDALONE_SIM
struct lc_header {
    unsigned long magic;
    unsigned short target_id;
    unsigned short api_version;
    unsigned char *load_addr;
    unsigned char *end_addr;
};
#define CP_NATIVE_TARGET_ID 71u
#else
#include "config.h"
#include "load_code.h"
#define CP_NATIVE_TARGET_ID TARGET_ID
#endif

struct cp_native_binary_header {
    struct lc_header lc_header;
    cp_native_entry_fn entry;
    unsigned char *bss_start;
    uint32_t host_api_size;
    uint32_t ui_api_size;
    uint32_t app_ops_size;
    uint16_t abi_minor;
    uint16_t react_profile;
};

extern const struct cp_native_app_ops *
cp_native_miniapp_entry(const struct cp_native_host_api *host);

#if (CONFIG_PLATFORM & PLATFORM_NATIVE)
extern unsigned char plugin_start_addr[];
extern unsigned char plugin_end_addr[];
extern unsigned char plugin_bss_start[];
#define CP_NATIVE_MINIAPP_HEADER                                        \
    const struct cp_native_binary_header __header                      \
        __attribute__((section(".header"))) = {                        \
            { CP_NATIVE_BINARY_MAGIC, CP_NATIVE_TARGET_ID,             \
              CP_NATIVE_ABI_MAJOR, plugin_start_addr,                  \
              plugin_end_addr },                                      \
            cp_native_miniapp_entry, plugin_bss_start,                 \
            sizeof(struct cp_native_host_api),                         \
            sizeof(struct cp_native_ui_api),                           \
            sizeof(struct cp_native_app_ops),                          \
            CP_NATIVE_ABI_MINOR, CP_NATIVE_REACT_PROFILE               \
        }
#else
#define CP_NATIVE_MINIAPP_HEADER                                        \
    const struct cp_native_binary_header __header                      \
        __attribute__((visibility("default"))) = {                     \
            { CP_NATIVE_BINARY_MAGIC, CP_NATIVE_TARGET_ID,             \
              CP_NATIVE_ABI_MAJOR, NULL, NULL },                       \
            cp_native_miniapp_entry, NULL,                             \
            sizeof(struct cp_native_host_api),                         \
            sizeof(struct cp_native_ui_api),                           \
            sizeof(struct cp_native_app_ops),                          \
            CP_NATIVE_ABI_MINOR, CP_NATIVE_REACT_PROFILE               \
        }
#endif
#endif

#endif
