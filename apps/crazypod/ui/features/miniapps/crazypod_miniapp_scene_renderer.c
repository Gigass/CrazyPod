#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "buflib.h"
#include "core_alloc.h"

#include "../../../crazypod_appearance.h"
#include "../../../crazypod_image.h"
#include "../../../crazypod_miniapps.h"
#include "../../../crazypod_miniapp_asset_font.h"
#include "../../../crazypod_miniapp_font.h"
#include "../../../crazypod_runtime_font.h"
#include "../../../crazypod_soundwave.h"
#include "../../../miniapps/runtime/crazypod_miniapp_host_system.h"
#include "../../features/now_playing/crazypod_now_playing_feature.h"
#include "../../presentation/crazypod_marquee.h"
#include "crazypod_miniapp_scene_internal.h"

static lv_obj_t *transform_target(
    struct crazypod_miniapp_scene_node *node)
{
    if(node != NULL && node->object != NULL &&
       node->type == CP_UI_OBJECT_NOW_PLAYING_ARTWORK)
        return lv_obj_get_child_count(node->object) > 0u
            ? lv_obj_get_child(node->object, 0) : NULL;
    return node != NULL ? node->object : NULL;
}

static bool artwork_transform_apply(
    struct crazypod_miniapp_scene_node *node)
{
    const fb_data *source;
    fb_data *output;
    lv_obj_t *image;
    int width;
    int height;
    int pivot_x;
    int pivot_y;
    int scale_x;
    int scale_y;
    int angle;
    int sine;
    int cosine;
    int y;

    if(node == NULL ||
       node->type != CP_UI_OBJECT_NOW_PLAYING_ARTWORK ||
       node->resource_handle <= 0 || node->secondary_handle <= 0)
        return false;
    image = transform_target(node);
    if(image == NULL)
        return false;
    width = node->values[CP_UI_PROP_WIDTH];
    height = node->values[CP_UI_PROP_HEIGHT];
    if(width <= 0 || height <= 0)
        return false;
    source = core_get_data(node->resource_handle);
    output = core_get_data(node->secondary_handle);
    if(source == NULL || output == NULL)
        return false;
    pivot_x = node->values[CP_UI_PROP_TRANSFORM_PIVOT_X];
    pivot_y = node->values[CP_UI_PROP_TRANSFORM_PIVOT_Y];
    scale_x = node->values[CP_UI_PROP_SCALE_X];
    scale_y = node->values[CP_UI_PROP_SCALE_Y];
    if(scale_x <= 0)
        scale_x = LV_SCALE_NONE;
    if(scale_y <= 0)
        scale_y = LV_SCALE_NONE;
    angle = node->values[CP_UI_PROP_ROTATION] % 3600;
    if(angle < 0)
        angle += 3600;
    if(angle == 0 && scale_x == LV_SCALE_NONE &&
       scale_y == LV_SCALE_NONE) {
        memcpy(output, source,
               (size_t)width * height * sizeof(fb_data));
    }
    else {
        int degrees = (angle + 5) / 10;

        sine = lv_trigo_sin(degrees);
        cosine = lv_trigo_cos(degrees);
        for(y = 0; y < height; ++y) {
            int x;

            for(x = 0; x < width; ++x) {
                int dx = x - pivot_x;
                int dy = y - pivot_y;
                int rotated_x = (int)(
                    ((int64_t)cosine * dx +
                     (int64_t)sine * dy) >> LV_TRIGO_SHIFT);
                int rotated_y = (int)(
                    ((int64_t)(-sine) * dx +
                     (int64_t)cosine * dy) >> LV_TRIGO_SHIFT);
                int source_x = pivot_x +
                    rotated_x * LV_SCALE_NONE / scale_x;
                int source_y = pivot_y +
                    rotated_y * LV_SCALE_NONE / scale_y;

                output[y * width + x] =
                    source_x >= 0 && source_x < width &&
                    source_y >= 0 && source_y < height
                    ? source[source_y * width + source_x]
                    : LCD_RGBPACK(18, 18, 24);
            }
        }
    }
    lv_obj_invalidate(image);
    node->artwork_transform_dirty = false;
    return true;
}

void crazypod_miniapp_scene_now_playing_artwork_transform_flush(
    struct crazypod_miniapp_scene_node *node)
{
    if(node != NULL && node->artwork_transform_dirty)
        (void)artwork_transform_apply(node);
}

bool crazypod_miniapp_scene_now_playing_artwork_refresh_node(
    struct crazypod_miniapp_scene_node *node)
{
    const lv_image_dsc_t *descriptor;
    lv_image_dsc_t staged_image;
    lv_image_dsc_t old_image;
    lv_obj_t *staged_object;
    int staged_handle;
    int staged_secondary_handle;
    int old_handle;
    int old_secondary_handle;
    uint32_t old_size;
    uint32_t old_secondary_size;
    unsigned generation;
    int width;
    int height;
    size_t pixel_count;
    size_t byte_count;

    if(node == NULL || node->object == NULL ||
       node->type != CP_UI_OBJECT_NOW_PLAYING_ARTWORK)
        return false;
    width = node->values[CP_UI_PROP_WIDTH];
    height = node->values[CP_UI_PROP_HEIGHT];
    if(width <= 0 || height <= 0)
        return false;
    descriptor = crazypod_now_playing_artwork_committed(
        NULL, &generation);
    if(node->artwork_generation == generation)
        return true;
    if(descriptor != NULL && descriptor->data != NULL &&
       descriptor->header.cf == LV_COLOR_FORMAT_RGB565 &&
       descriptor->header.w > 0 && descriptor->header.h > 0) {
        const fb_data *source = (const fb_data *)descriptor->data;
        int source_stride =
            descriptor->header.stride / sizeof(fb_data);
        int crop_x = 0;
        int crop_y = 0;
        int crop_width = descriptor->header.w;
        int crop_height = descriptor->header.h;
        pixel_count = (size_t)width * (size_t)height;
        byte_count = pixel_count * sizeof(fb_data);

        if((int64_t)crop_width * height >
           (int64_t)crop_height * width) {
            crop_width = crop_height * width / height;
            crop_x = (descriptor->header.w - crop_width) / 2;
        }
        else {
            crop_height = crop_width * height / width;
            crop_y = (descriptor->header.h - crop_height) / 2;
        }
        if(crop_width <= 0 || crop_height <= 0 ||
           byte_count == 0 || byte_count > UINT32_MAX ||
           byte_count > SIZE_MAX / 2u ||
           !crazypod_miniapp_host_memory_replace(
               (size_t)node->external_size + node->secondary_size,
               byte_count * 2u))
            return false;
        staged_handle = core_alloc_ex(
            byte_count, &buflib_ops_locked);
        if(staged_handle <= 0) {
            (void)crazypod_miniapp_host_memory_replace(
                byte_count * 2u,
                (size_t)node->external_size + node->secondary_size);
            return false;
        }
        staged_secondary_handle = core_alloc_ex(
            byte_count, &buflib_ops_locked);
        if(staged_secondary_handle <= 0) {
            core_free(staged_handle);
            (void)crazypod_miniapp_host_memory_replace(
                byte_count * 2u,
                (size_t)node->external_size + node->secondary_size);
            return false;
        }
        memset(&staged_image, 0, sizeof(staged_image));
        if(!crazypod_image_scale_rgb565(
               source + crop_y * source_stride + crop_x,
               crop_width, crop_height, source_stride,
               core_get_data(staged_handle), width, height) ||
           !crazypod_image_configure_rgb565(
               &staged_image, core_get_data(staged_secondary_handle),
               width, height)) {
            core_free(staged_handle);
            core_free(staged_secondary_handle);
            (void)crazypod_miniapp_host_memory_replace(
                byte_count * 2u,
                (size_t)node->external_size + node->secondary_size);
            return false;
        }
        memcpy(core_get_data(staged_secondary_handle),
               core_get_data(staged_handle), byte_count);
        old_handle = node->resource_handle;
        old_secondary_handle = node->secondary_handle;
        old_size = node->external_size;
        old_secondary_size = node->secondary_size;
        old_image = node->image;
        node->image = staged_image;
        staged_object = lv_image_create(node->object);
        if(staged_object == NULL) {
            node->image = old_image;
            core_free(staged_handle);
            core_free(staged_secondary_handle);
            (void)crazypod_miniapp_host_memory_replace(
                byte_count * 2u,
                (size_t)old_size + old_secondary_size);
            return false;
        }
        lv_image_set_src(staged_object, &node->image);
        lv_obj_set_pos(staged_object, 0, 0);
        lv_obj_set_style_translate_x(
            staged_object, node->values[CP_UI_PROP_TRANSLATE_X], 0);
        lv_obj_set_style_translate_y(
            staged_object, node->values[CP_UI_PROP_TRANSLATE_Y], 0);
        lv_obj_remove_flag(staged_object, LV_OBJ_FLAG_CLICKABLE);
        while(lv_obj_get_child_count(node->object) > 1u) {
            lv_obj_t *child = lv_obj_get_child(node->object, 0);

            if(child == staged_object)
                child = lv_obj_get_child(node->object, 1);
            lv_obj_delete(child);
        }
        node->resource_handle = staged_handle;
        node->external_size = (uint32_t)byte_count;
        node->secondary_handle = staged_secondary_handle;
        node->secondary_size = (uint32_t)byte_count;
        node->artwork_generation = generation;
        (void)artwork_transform_apply(node);
        if(old_handle > 0)
            core_free(old_handle);
        if(old_secondary_handle > 0)
            core_free(old_secondary_handle);
        lv_obj_set_style_bg_opa(node->object, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(
            node->object, lv_color_hex(0x121218), 0);
        lv_obj_set_style_clip_corner(node->object, true, 0);
        lv_obj_invalidate(node->object);
        return true;
    }
    return false;
}

bool crazypod_miniapp_scene_now_playing_artwork_needs_refresh(
    struct crazypod_miniapp_scene_node *node)
{
    unsigned generation;

    if(node == NULL ||
       node->type != CP_UI_OBJECT_NOW_PLAYING_ARTWORK)
        return false;
    (void)crazypod_now_playing_artwork_committed(
        NULL, &generation);
    return node->artwork_generation != generation;
}

static void draw_sound_wave(lv_event_t *event)
{
    struct crazypod_miniapp_scene_node *node =
        lv_event_get_user_data(event);
    const struct crazypod_appearance *appearance =
        crazypod_appearance_get();
    lv_area_t area;
    int style;

    if(node == NULL || lv_event_get_code(event) != LV_EVENT_DRAW_MAIN)
        return;
    style = node->values[CP_UI_PROP_WAVE_STYLE];
    if(style < 0 || style >= CRAZYPOD_SOUND_WAVE_STYLE_COUNT)
        style = appearance->sound_wave_style;
    lv_obj_get_coords(node->object, &area);
    crazypod_sound_wave_draw_bar(
        lv_event_get_layer(event), &area,
        (enum crazypod_sound_wave_style)style,
        node->values[CP_UI_PROP_PHASE],
        node->values[CP_UI_PROP_PLAYING] != 0,
        crazypod_appearance_color(appearance->primary_color),
        crazypod_appearance_color(appearance->secondary_color),
        0xFFFFFF);
}

static enum crazypod_font_family family_for_value(int32_t value)
{
    switch(value) {
    case CP_UI_FONT_SERIF_14:
    case CP_UI_FONT_SERIF_28:
    case CP_UI_FONT_SERIF:
        return CRAZYPOD_FONT_FAMILY_SERIF;
    case CP_UI_FONT_TECHNICAL_8:
    case CP_UI_FONT_TECHNICAL_16:
    case CP_UI_FONT_MONO:
        return CRAZYPOD_FONT_FAMILY_MONO;
    default:
        /* Removed design-font IDs remain binary-compatible, but old
         * packages now use the same Noto system family. */
        return CRAZYPOD_FONT_FAMILY_SYSTEM;
    }
}

static const lv_font_t *font_for_value(
    int32_t value, int32_t size, int32_t weight,
    int32_t style, int32_t line_height)
{
    const lv_font_t *font;

    if(size < 6 || size > 48)
        size = 12;
    if(weight < 100 || weight > 900 || weight % 100 != 0)
        weight = 400;
    if(line_height < size)
        line_height = 0;
    font = crazypod_runtime_font_resolve(
        family_for_value(value), (unsigned)size, (unsigned)weight,
        style == CP_UI_FONT_STYLE_ITALIC
            ? CRAZYPOD_FONT_STYLE_ITALIC
            : CRAZYPOD_FONT_STYLE_NORMAL,
        (unsigned)line_height);
    return font;
}

static lv_flex_align_t flex_align(int32_t value)
{
    switch(value) {
    case CP_UI_PLACE_CENTER:
        return LV_FLEX_ALIGN_CENTER;
    case CP_UI_PLACE_END:
        return LV_FLEX_ALIGN_END;
    case CP_UI_PLACE_SPACE_BETWEEN:
        return LV_FLEX_ALIGN_SPACE_BETWEEN;
    case CP_UI_PLACE_SPACE_AROUND:
        return LV_FLEX_ALIGN_SPACE_AROUND;
    case CP_UI_PLACE_SPACE_EVENLY:
        return LV_FLEX_ALIGN_SPACE_EVENLY;
    default:
        return LV_FLEX_ALIGN_START;
    }
}

static void apply_range(struct crazypod_miniapp_scene_node *node)
{
    int32_t minimum = node->values[CP_UI_PROP_MINIMUM];
    int32_t maximum = node->values[CP_UI_PROP_MAXIMUM];

    if(maximum <= minimum)
        maximum = minimum + 1;
    switch(node->type) {
    case CP_UI_OBJECT_PROGRESS:
        lv_bar_set_range(node->object, minimum, maximum);
        break;
    case CP_UI_OBJECT_ARC:
        lv_arc_set_range(node->object, minimum, maximum);
        break;
    case CP_UI_OBJECT_SLIDER:
        lv_slider_set_range(node->object, minimum, maximum);
        break;
    case CP_UI_OBJECT_CHART:
        lv_chart_set_range(
            node->object, LV_CHART_AXIS_PRIMARY_Y,
            minimum, maximum);
        break;
    default:
        break;
    }
}

static void apply_text(struct crazypod_miniapp_scene_node *node)
{
    switch(node->type) {
    case CP_UI_OBJECT_TEXT:
        if(node->values[CP_UI_PROP_MARQUEE])
            crazypod_marquee_set_text(
                node->object, node->text, true);
        else
            lv_label_set_text(node->object, node->text);
        break;
    case CP_UI_OBJECT_TEXT_INPUT:
        lv_textarea_set_text(node->object, node->text);
        break;
    case CP_UI_OBJECT_CHECKBOX:
        lv_checkbox_set_text(node->object, node->text);
        break;
    case CP_UI_OBJECT_DROPDOWN:
        lv_dropdown_set_options(node->object, node->text);
        break;
    case CP_UI_OBJECT_ROLLER:
        lv_roller_set_options(
            node->object, node->text, LV_ROLLER_MODE_NORMAL);
        break;
    default:
        break;
    }
}

static void apply_number_of_lines(
    struct crazypod_miniapp_scene_node *node)
{
    const lv_font_t *font;
    int32_t lines;

    if(node->type != CP_UI_OBJECT_TEXT)
        return;
    lines = node->values[CP_UI_PROP_NUMBER_OF_LINES];
    if(lines <= 0) {
        lv_label_set_long_mode(node->object, LV_LABEL_LONG_MODE_WRAP);
        return;
    }
    if(lines > 8)
        lines = 8;
    font = lv_obj_get_style_text_font(node->object, LV_PART_MAIN);
    if(font != NULL && font->line_height > 0)
        lv_obj_set_height(node->object, font->line_height * lines);
    if(node->values[CP_UI_PROP_MARQUEE])
        crazypod_marquee_configure(node->object, true);
    else
        lv_label_set_long_mode(
            node->object, LV_LABEL_LONG_MODE_DOTS);
}

static void apply_text_font(
    struct crazypod_miniapp_scene_node *node,
    const lv_font_t *font)
{
    if(font == NULL)
        return;
    lv_obj_set_style_text_font(node->object, font, 0);
    if(node->values[CP_UI_PROP_NUMBER_OF_LINES] > 0)
        apply_number_of_lines(node);
    else if(node->type == CP_UI_OBJECT_TEXT &&
            node->values[CP_UI_PROP_MARQUEE])
        crazypod_marquee_configure(node->object, true);
}

bool crazypod_miniapp_scene_text_font_apply(
    struct crazypod_miniapp_scene_node *node, int32_t value)
{
    const lv_font_t *font;

    if(node == NULL)
        return false;
    font = font_for_value(
        value, node->values[CP_UI_PROP_FONT_SIZE],
        node->values[CP_UI_PROP_FONT_WEIGHT],
        node->values[CP_UI_PROP_FONT_STYLE],
        node->values[CP_UI_PROP_LINE_HEIGHT]);
    if(font == NULL)
        return false;
    if(node->object != NULL)
        apply_text_font(node, font);
    return true;
}

static void apply_grid(struct crazypod_miniapp_scene_node *node)
{
    int32_t columns = node->values[CP_UI_PROP_GRID_COLUMNS];
    int32_t rows = node->values[CP_UI_PROP_GRID_ROWS];
    int index;

    if(columns < 1)
        columns = 1;
    if(rows < 1)
        rows = 1;
    if(columns > 8)
        columns = 8;
    if(rows > 8)
        rows = 8;
    for(index = 0; index < columns; ++index)
        node->grid_column_descriptors[index] = LV_GRID_FR(1);
    node->grid_column_descriptors[columns] = LV_GRID_TEMPLATE_LAST;
    for(index = 0; index < rows; ++index)
        node->grid_row_descriptors[index] = LV_GRID_FR(1);
    node->grid_row_descriptors[rows] = LV_GRID_TEMPLATE_LAST;
    lv_obj_set_grid_dsc_array(
        node->object,
        node->grid_column_descriptors,
        node->grid_row_descriptors);
    crazypod_miniapp_scene_grid_refresh(node);
}

void crazypod_miniapp_scene_grid_refresh(
    struct crazypod_miniapp_scene_node *node)
{
    int32_t columns;
    int32_t rows;
    uint32_t count;
    uint32_t index;

    if(node == NULL || node->object == NULL ||
       node->values[CP_UI_PROP_LAYOUT] != CP_UI_LAYOUT_GRID)
        return;
    columns = node->values[CP_UI_PROP_GRID_COLUMNS];
    rows = node->values[CP_UI_PROP_GRID_ROWS];
    if(columns < 1)
        columns = 1;
    if(rows < 1)
        rows = 1;
    if(columns > 8)
        columns = 8;
    if(rows > 8)
        rows = 8;
    count = lv_obj_get_child_count(node->object);
    for(index = 0; index < count; ++index) {
        int32_t column = (int32_t)(index % (uint32_t)columns);
        int32_t row = (int32_t)(index / (uint32_t)columns);
        lv_obj_t *child = lv_obj_get_child(
            node->object, (int32_t)index);

        if(row >= rows)
            row = rows - 1;
        lv_obj_set_grid_cell(
            child,
            LV_GRID_ALIGN_STRETCH, column, 1,
            LV_GRID_ALIGN_STRETCH, row, 1);
    }
}

void crazypod_miniapp_scene_node_release(
    struct crazypod_miniapp_scene_node *node)
{
    if(node == NULL)
        return;
    if(node->object != NULL &&
       node->type == CP_UI_OBJECT_ANIMATED_IMAGE) {
        lv_animimg_delete(node->object);
        lv_animimg_set_src(node->object, NULL, 0);
        lv_image_set_src(node->object, NULL);
    }
    else if(node->object != NULL &&
            node->type == CP_UI_OBJECT_IMAGE)
        lv_image_set_src(node->object, NULL);
    else if(node->object != NULL &&
            node->type == CP_UI_OBJECT_IMAGE_BUTTON) {
        lv_imagebutton_state_t state;

        for(state = LV_IMAGEBUTTON_STATE_RELEASED;
            state < LV_IMAGEBUTTON_STATE_NUM; ++state)
            lv_imagebutton_set_src(
                node->object, state, NULL, NULL, NULL);
    }
    if(node->resource_handle > 0)
        core_free(node->resource_handle);
    if(node->secondary_handle > 0)
        core_free(node->secondary_handle);
    if(node->data_handle > 0)
        core_free(node->data_handle);
    crazypod_miniapp_host_memory_release(node->external_size);
    node->resource_handle = 0;
    node->external_size = 0;
    crazypod_miniapp_host_memory_release(node->secondary_size);
    node->secondary_handle = 0;
    node->secondary_size = 0;
    crazypod_miniapp_host_memory_release(node->data_size);
    node->data_handle = 0;
    node->data_size = 0;
    node->chart_series = NULL;
    node->table_columns = 0;
    node->secondary_source[0] = '\0';
    memset(&node->image, 0, sizeof(node->image));
}

bool crazypod_miniapp_scene_data_replace(
    struct crazypod_miniapp_scene_node *node,
    const void *data, size_t size)
{
    int handle;

    if(data == NULL || size == 0 ||
       size > CP_NATIVE_UI_PAYLOAD_MAX)
        return false;
    if(!crazypod_miniapp_host_memory_reserve(size))
        return false;
    handle = core_alloc_ex(size, &buflib_ops_locked);
    if(handle <= 0) {
        crazypod_miniapp_host_memory_release(size);
        return false;
    }
    memcpy(core_get_data(handle), data, size);
    if(node->data_handle > 0)
        core_free(node->data_handle);
    crazypod_miniapp_host_memory_release(node->data_size);
    node->data_handle = handle;
    node->data_size = size;
    return true;
}

static bool apply_chart_data(
    struct crazypod_miniapp_scene_node *node)
{
    struct cp_chart_data_header header;
    const int16_t *points;
    size_t expected;
    uint16_t index;

    if(node->data_handle <= 0 ||
       node->data_size < sizeof(header))
        return false;
    memcpy(
        &header, core_get_data(node->data_handle),
        sizeof(header));
    expected = sizeof(header) +
        (size_t)header.point_count * sizeof(*points);
    if(header.magic != CP_CHART_DATA_MAGIC ||
       header.reserved != 0 ||
       header.point_count == 0 ||
       header.point_count > CP_CHART_POINT_MAX ||
       expected != node->data_size)
        return false;
    if(node->object == NULL)
        return true;
    if(node->chart_series == NULL) {
        node->chart_series = lv_chart_add_series(
            node->object,
            lv_color_hex(0xff9f43),
            LV_CHART_AXIS_PRIMARY_Y);
        if(node->chart_series == NULL)
            return false;
    }
    points = (const int16_t *)(
        (const uint8_t *)core_get_data(node->data_handle) +
        sizeof(header));
    lv_chart_set_type(node->object, LV_CHART_TYPE_LINE);
    lv_obj_set_style_line_width(
        node->object, 2, LV_PART_ITEMS);
    lv_obj_set_style_line_opa(
        node->object, LV_OPA_COVER, LV_PART_ITEMS);
    lv_chart_set_point_count(
        node->object, header.point_count);
    lv_chart_set_all_values(
        node->object, node->chart_series, LV_CHART_POINT_NONE);
    lv_chart_set_x_start_point(
        node->object, node->chart_series, 0);
    for(index = 0; index < header.point_count; ++index)
        lv_chart_set_next_value(
            node->object, node->chart_series, points[index]);
    lv_chart_refresh(node->object);
    return true;
}

static void apply_table_widths(
    struct crazypod_miniapp_scene_node *node)
{
    int32_t width;
    uint8_t column;

    if(node->object == NULL ||
       node->type != CP_UI_OBJECT_TABLE ||
       node->table_columns == 0)
        return;
    width = node->values[CP_UI_PROP_WIDTH];
    if(width <= 0)
        return;
    for(column = 0; column < node->table_columns; ++column)
        lv_table_set_column_width(
            node->object, column,
            width / node->table_columns);
}

static bool apply_table_data(
    struct crazypod_miniapp_scene_node *node)
{
    struct cp_table_data_header header;
    const uint8_t *bytes;
    size_t cursor;
    uint16_t row;
    uint16_t column;

    if(node->data_handle <= 0 ||
       node->data_size < sizeof(header))
        return false;
    bytes = core_get_data(node->data_handle);
    memcpy(&header, bytes, sizeof(header));
    if(header.magic != CP_TABLE_DATA_MAGIC ||
       header.total_size != node->data_size ||
       header.row_count == 0 ||
       header.row_count > CP_TABLE_ROW_MAX ||
       header.column_count == 0 ||
       header.column_count > CP_TABLE_COLUMN_MAX)
        return false;
    cursor = sizeof(header);
    for(row = 0; row < header.row_count; ++row)
        for(column = 0;
            column < header.column_count; ++column) {
            uint8_t length;

            if(cursor >= node->data_size)
                return false;
            length = bytes[cursor++];
            if(length > CP_TABLE_CELL_MAX ||
               length > node->data_size - cursor ||
               memchr(bytes + cursor, '\0', length) != NULL)
                return false;
            cursor += length;
        }
    if(cursor != node->data_size)
        return false;
    node->table_columns = (uint8_t)header.column_count;
    if(node->object == NULL)
        return true;
    lv_table_set_row_count(node->object, header.row_count);
    lv_table_set_column_count(
        node->object, header.column_count);
    cursor = sizeof(header);
    for(row = 0; row < header.row_count; ++row)
        for(column = 0;
            column < header.column_count; ++column) {
            char cell[CP_TABLE_CELL_MAX + 1u];
            uint8_t length = bytes[cursor++];

            memcpy(cell, bytes + cursor, length);
            cell[length] = '\0';
            cursor += length;
            lv_table_set_cell_value(
                node->object, row, column, cell);
        }
    apply_table_widths(node);
    return true;
}

bool crazypod_miniapp_scene_data_apply(
    struct crazypod_miniapp_scene_node *node)
{
    if(node->type == CP_UI_OBJECT_CHART)
        return apply_chart_data(node);
    if(node->type == CP_UI_OBJECT_TABLE)
        return apply_table_data(node);
    return false;
}

static void apply_image_source(
    struct crazypod_miniapp_scene_node *node)
{
    struct cp_resource_info info = {
        .struct_size = sizeof(info),
    };
    void *pixels;

    if(node->type != CP_UI_OBJECT_IMAGE &&
       node->type != CP_UI_OBJECT_IMAGE_BUTTON &&
       node->type != CP_UI_OBJECT_ANIMATED_IMAGE)
        return;
    crazypod_miniapp_scene_node_release(node);
    if(node->source[0] == '\0')
        return;
    if(crazypod_miniapps_resource_stat(
           node->source, &info) != CRAZYPOD_MINIAPP_OK ||
       (info.type != CP_RESOURCE_BITMAP_RGB565 &&
        info.type != CP_RESOURCE_SPRITE_SHEET) ||
       info.width == 0 || info.height == 0 ||
       info.size != (uint32_t)info.width * info.height * 2u)
        return;
    if(!crazypod_miniapp_host_memory_reserve(info.size))
        return;
    node->resource_handle = core_alloc_ex(
        info.size, &buflib_ops_locked);
    if(node->resource_handle <= 0) {
        node->resource_handle = 0;
        crazypod_miniapp_host_memory_release(info.size);
        return;
    }
    node->external_size = info.size;
    pixels = core_get_data(node->resource_handle);
    if(crazypod_miniapps_resource_read(
           node->source, 0, pixels, info.size) != (int)info.size) {
        core_free(node->resource_handle);
        node->resource_handle = 0;
        node->external_size = 0;
        crazypod_miniapp_host_memory_release(info.size);
        return;
    }
    node->image.header.magic = LV_IMAGE_HEADER_MAGIC;
    node->image.header.cf = LV_COLOR_FORMAT_RGB565;
    node->image.header.w = info.width;
    node->image.header.h = info.height;
    node->image.header.stride = info.width * 2u;
    node->image.data_size = info.size;
    node->image.data = pixels;
    if(node->type == CP_UI_OBJECT_IMAGE) {
        lv_image_set_src(node->object, &node->image);
    }
    else if(node->type == CP_UI_OBJECT_IMAGE_BUTTON) {
        lv_imagebutton_state_t state;

        for(state = LV_IMAGEBUTTON_STATE_RELEASED;
            state < LV_IMAGEBUTTON_STATE_NUM; ++state)
            lv_imagebutton_set_src(
                node->object, state,
                NULL, &node->image, NULL);
    }
    else {
        uint8_t frame_count =
            info.type == CP_RESOURCE_SPRITE_SHEET
                ? info.frame_count : 1;
        uint16_t frame_height;
        uint32_t frame_size;
        size_t sources_size;
        size_t workspace_size;
        const void **sources;
        lv_image_dsc_t *frames;
        uint8_t frame;

        if(frame_count == 0 || frame_count > 32 ||
           info.height % frame_count != 0) {
            crazypod_miniapp_scene_node_release(node);
            return;
        }
        frame_height = info.height / frame_count;
        frame_size = (uint32_t)info.width * frame_height * 2u;
        sources_size = (size_t)frame_count * sizeof(*sources);
        workspace_size = sources_size +
            (size_t)frame_count * sizeof(*frames);
        if(!crazypod_miniapp_host_memory_reserve(workspace_size)) {
            crazypod_miniapp_scene_node_release(node);
            return;
        }
        node->secondary_handle = core_alloc_ex(
            workspace_size, &buflib_ops_locked);
        if(node->secondary_handle <= 0) {
            node->secondary_handle = 0;
            crazypod_miniapp_host_memory_release(workspace_size);
            crazypod_miniapp_scene_node_release(node);
            return;
        }
        node->secondary_size = workspace_size;
        sources = core_get_data(node->secondary_handle);
        frames = (lv_image_dsc_t *)((uint8_t *)sources + sources_size);
        memset(frames, 0, (size_t)frame_count * sizeof(*frames));
        for(frame = 0; frame < frame_count; ++frame) {
            frames[frame].header.magic = LV_IMAGE_HEADER_MAGIC;
            frames[frame].header.cf = LV_COLOR_FORMAT_RGB565;
            frames[frame].header.w = info.width;
            frames[frame].header.h = frame_height;
            frames[frame].header.stride = info.width * 2u;
            frames[frame].data_size = frame_size;
            frames[frame].data =
                (const uint8_t *)pixels + (size_t)frame * frame_size;
            sources[frame] = &frames[frame];
        }
        lv_animimg_set_src(node->object, sources, frame_count);
        lv_animimg_set_duration(
            node->object,
            (uint32_t)(info.frame_duration_ms != 0
                ? info.frame_duration_ms : 100u) * frame_count);
        lv_animimg_set_repeat_count(
            node->object, LV_ANIM_REPEAT_INFINITE);
        lv_image_set_src(node->object, sources[0]);
        if(frame_count > 1)
            lv_animimg_start(node->object);
    }
}

static bool canvas_allocate(
    struct crazypod_miniapp_scene_node *node)
{
    int32_t width = node->values[CP_UI_PROP_WIDTH];
    int32_t height = node->values[CP_UI_PROP_HEIGHT];
    size_t size;
    void *pixels;

    if((node->type != CP_UI_OBJECT_CANVAS &&
        node->type != CP_UI_OBJECT_TILEMAP) ||
       width <= 0 || width > 320 ||
       height <= 0 || height > 240)
        return false;
    size = (size_t)width * (size_t)height * 2u;
    if(node->resource_handle > 0 &&
       node->external_size == size) {
        lv_canvas_set_buffer(
            node->object,
            core_get_data(node->resource_handle),
            width, height, LV_COLOR_FORMAT_RGB565);
        return true;
    }
    crazypod_miniapp_scene_node_release(node);
    if(!crazypod_miniapp_host_memory_reserve(size))
        return false;
    node->resource_handle = core_alloc_ex(
        size, &buflib_ops_locked);
    if(node->resource_handle <= 0) {
        node->resource_handle = 0;
        crazypod_miniapp_host_memory_release(size);
        return false;
    }
    node->external_size = size;
    pixels = core_get_data(node->resource_handle);
    memset(pixels, 0, size);
    lv_canvas_set_buffer(
        node->object, pixels, width, height,
        LV_COLOR_FORMAT_RGB565);
    return true;
}

static bool tilemap_load_tileset(
    struct crazypod_miniapp_scene_node *node,
    const char *id, struct cp_resource_info *info)
{
    void *pixels;

    if(node->secondary_handle > 0 &&
       strcmp(node->secondary_source, id) == 0)
        return crazypod_miniapps_resource_stat(id, info) ==
            CRAZYPOD_MINIAPP_OK;
    if(node->secondary_handle > 0) {
        core_free(node->secondary_handle);
        crazypod_miniapp_host_memory_release(
            node->secondary_size);
        node->secondary_handle = 0;
        node->secondary_size = 0;
        node->secondary_source[0] = '\0';
    }
    if(crazypod_miniapps_resource_stat(id, info) !=
           CRAZYPOD_MINIAPP_OK ||
       info->type != CP_RESOURCE_TILESET ||
       info->size != (uint32_t)info->width * info->height * 2u ||
       !crazypod_miniapp_host_memory_reserve(info->size))
        return false;
    node->secondary_handle = core_alloc_ex(
        info->size, &buflib_ops_locked);
    if(node->secondary_handle <= 0) {
        node->secondary_handle = 0;
        crazypod_miniapp_host_memory_release(info->size);
        return false;
    }
    pixels = core_get_data(node->secondary_handle);
    if(crazypod_miniapps_resource_read(
           id, 0, pixels, info->size) != (int)info->size) {
        core_free(node->secondary_handle);
        node->secondary_handle = 0;
        crazypod_miniapp_host_memory_release(info->size);
        return false;
    }
    node->secondary_size = info->size;
    snprintf(
        node->secondary_source,
        sizeof(node->secondary_source), "%s", id);
    return true;
}

static void tilemap_draw_tile(
    struct crazypod_miniapp_scene_node *node,
    const uint16_t *tileset, uint16_t tileset_width,
    uint16_t tileset_height, uint8_t tile_width,
    uint8_t tile_height, uint16_t tile,
    int32_t destination_x, int32_t destination_y,
    uint16_t flags)
{
    int32_t canvas_width = node->values[CP_UI_PROP_WIDTH];
    int32_t canvas_height = node->values[CP_UI_PROP_HEIGHT];
    uint16_t columns = tileset_width / tile_width;
    uint16_t rows = tileset_height / tile_height;
    uint16_t *canvas = core_get_data(node->resource_handle);
    uint16_t source_x;
    uint16_t source_y;
    int32_t y;

    if(columns == 0 || rows == 0 ||
       tile >= (uint16_t)(columns * rows) ||
       destination_x >= canvas_width ||
       destination_y >= canvas_height ||
       destination_x + tile_width <= 0 ||
       destination_y + tile_height <= 0)
        return;
    source_x = (tile % columns) * tile_width;
    source_y = (tile / columns) * tile_height;
    for(y = 0; y < tile_height; ++y) {
        int32_t target_y = destination_y + y;
        int32_t x;

        if(target_y < 0 || target_y >= canvas_height)
            continue;
        for(x = 0; x < tile_width; ++x) {
            int32_t target_x = destination_x + x;
            uint16_t sample_x = (uint16_t)((flags & 1u)
                ? (int32_t)tile_width - 1 - x : x);
            uint16_t sample_y = (uint16_t)((flags & 2u)
                ? (int32_t)tile_height - 1 - y : y);
            uint16_t color;

            if(target_x < 0 || target_x >= canvas_width)
                continue;
            color = tileset[
                (size_t)(source_y + sample_y) * tileset_width +
                source_x + sample_x];
            if((flags & 4u) != 0 && color == 0xf81fu)
                continue;
            canvas[
                (size_t)target_y * canvas_width + target_x] = color;
        }
    }
}

bool crazypod_miniapp_scene_tilemap_commit(
    struct crazypod_miniapp_scene_node *node,
    const void *data, size_t size)
{
    struct cp_tilemap_header header;
    struct cp_resource_info tileset_info = {
        .struct_size = sizeof(tileset_info),
    };
    const uint8_t *bytes = data;
    const uint16_t *layers;
    const struct cp_tilemap_sprite *sprites;
    const uint16_t *tileset;
    size_t tile_count;
    size_t expected;
    uint8_t layer;

    if(node == NULL || node->type != CP_UI_OBJECT_TILEMAP ||
       data == NULL || size < sizeof(header) ||
       !canvas_allocate(node))
        return false;
    memcpy(&header, data, sizeof(header));
    if(header.magic != CP_TILEMAP_MAGIC ||
       header.version != CP_TILEMAP_ABI ||
       header.header_size != CP_TILEMAP_HEADER_SIZE ||
       header.total_size != size ||
       header.map_width == 0 ||
       header.map_width > CP_TILEMAP_WIDTH_MAX ||
       header.map_height == 0 ||
       header.map_height > CP_TILEMAP_HEIGHT_MAX ||
       header.tile_width < 8 || header.tile_width > 32 ||
       header.tile_height < 8 || header.tile_height > 32 ||
       header.layer_count == 0 ||
       header.layer_count > CP_TILEMAP_LAYER_MAX ||
       header.sprite_count > CP_TILEMAP_SPRITE_MAX ||
       memchr(header.tileset, '\0', sizeof(header.tileset)) == NULL)
        return false;
    tile_count =
        (size_t)header.map_width * header.map_height;
    expected = sizeof(header) +
        tile_count * header.layer_count * sizeof(uint16_t) +
        (size_t)header.sprite_count *
            sizeof(struct cp_tilemap_sprite);
    if(expected != size ||
       !tilemap_load_tileset(
           node, header.tileset, &tileset_info) ||
       tileset_info.width % header.tile_width != 0 ||
       tileset_info.height % header.tile_height != 0)
        return false;
    layers = (const uint16_t *)(bytes + sizeof(header));
    sprites = (const struct cp_tilemap_sprite *)(
        layers + tile_count * header.layer_count);
    tileset = core_get_data(node->secondary_handle);
    memset(
        core_get_data(node->resource_handle), 0,
        node->external_size);
    for(layer = 0; layer < header.layer_count; ++layer) {
        size_t index;

        for(index = 0; index < tile_count; ++index) {
            uint16_t tile =
                layers[(size_t)layer * tile_count + index];
            int32_t column = index % header.map_width;
            int32_t row = index / header.map_width;

            if(tile == 0xffffu)
                continue;
            tilemap_draw_tile(
                node, tileset,
                tileset_info.width, tileset_info.height,
                header.tile_width, header.tile_height, tile,
                column * header.tile_width - header.camera_x,
                row * header.tile_height - header.camera_y, 0);
        }
    }
    {
        uint8_t index;
        for(index = 0; index < header.sprite_count; ++index)
            tilemap_draw_tile(
                node, tileset,
                tileset_info.width, tileset_info.height,
                header.tile_width, header.tile_height,
                sprites[index].tile,
                sprites[index].x - header.camera_x,
                sprites[index].y - header.camera_y,
                sprites[index].flags);
    }
    lv_obj_invalidate(node->object);
    return true;
}

static void canvas_pixel(
    struct crazypod_miniapp_scene_node *node,
    int32_t x, int32_t y, uint16_t color, uint8_t opacity,
    int32_t clip_left, int32_t clip_top,
    int32_t clip_right, int32_t clip_bottom)
{
    int32_t width = node->values[CP_UI_PROP_WIDTH];
    int32_t height = node->values[CP_UI_PROP_HEIGHT];
    uint16_t *pixels;
    uint16_t *destination;

    if(x < 0 || x >= width || y < 0 || y >= height ||
       x < clip_left || x >= clip_right ||
       y < clip_top || y >= clip_bottom ||
       opacity == 0)
        return;
    pixels = core_get_data(node->resource_handle);
    destination =
        &pixels[(size_t)y * (size_t)width + (size_t)x];
    if(opacity == LV_OPA_COVER) {
        *destination = color;
    }
    else {
        uint32_t inverse = 255u - opacity;
        uint32_t source_red = (color >> 11) & 0x1fu;
        uint32_t source_green = (color >> 5) & 0x3fu;
        uint32_t source_blue = color & 0x1fu;
        uint32_t target_red = (*destination >> 11) & 0x1fu;
        uint32_t target_green = (*destination >> 5) & 0x3fu;
        uint32_t target_blue = *destination & 0x1fu;
        uint16_t red = (uint16_t)(
            (source_red * opacity +
             target_red * inverse + 127u) / 255u);
        uint16_t green = (uint16_t)(
            (source_green * opacity +
             target_green * inverse + 127u) / 255u);
        uint16_t blue = (uint16_t)(
            (source_blue * opacity +
             target_blue * inverse + 127u) / 255u);

        *destination =
            (uint16_t)((red << 11) | (green << 5) | blue);
    }
}

static bool canvas_payload(
    const struct cp_canvas_header *header,
    const struct cp_canvas_command *command,
    const uint8_t *bytes, const uint8_t **payload)
{
    if(command->payload_offset > header->payload_size ||
       command->payload_size >
           header->payload_size - command->payload_offset)
        return false;
    *payload = bytes + header->payload_offset +
        command->payload_offset;
    return true;
}

static bool canvas_draw_text(
    struct crazypod_miniapp_scene_node *node,
    const struct cp_canvas_command *command,
    const uint8_t *payload,
    int32_t clip_left, int32_t clip_top,
    int32_t clip_right, int32_t clip_bottom)
{
    lv_draw_label_dsc_t descriptor;
    const lv_font_t *font;
    lv_layer_t layer;
    lv_area_t coordinates;
    char text[CRAZYPOD_MINIAPP_TEXT_CAPACITY];

    if(command->payload_size == 0 ||
       command->payload_size >= sizeof(text) ||
       command->flags >= CP_UI_FONT_COUNT ||
       memchr(payload, '\0', command->payload_size) != NULL)
        return false;
    if(clip_right <= clip_left || clip_bottom <= clip_top)
        return true;
    memcpy(text, payload, command->payload_size);
    text[command->payload_size] = '\0';
    lv_canvas_init_layer(node->object, &layer);
    layer._clip_area.x1 = clip_left;
    layer._clip_area.y1 = clip_top;
    layer._clip_area.x2 = clip_right - 1;
    layer._clip_area.y2 = clip_bottom - 1;
    font = font_for_value(
        command->flags, 12, 400, CP_UI_FONT_STYLE_NORMAL, 0);
    if(font == NULL)
        return false;
    lv_draw_label_dsc_init(&descriptor);
    descriptor.text = text;
    descriptor.text_length = command->payload_size;
    descriptor.font = font;
    descriptor.color =
        lv_color_hex(command->color & 0xffffffu);
    descriptor.opa = command->opacity;
    coordinates.x1 = command->x;
    coordinates.y1 = command->y;
    coordinates.x2 = command->width > 0
        ? command->x + command->width - 1
        : node->values[CP_UI_PROP_WIDTH] - 1;
    coordinates.y2 = command->height > 0
        ? command->y + command->height - 1
        : node->values[CP_UI_PROP_HEIGHT] - 1;
    lv_draw_label(&layer, &descriptor, &coordinates);
    lv_canvas_finish_layer(node->object, &layer);
    return true;
}

static bool canvas_draw_image(
    struct crazypod_miniapp_scene_node *node,
    const struct cp_canvas_command *command,
    const uint8_t *payload,
    int32_t clip_left, int32_t clip_top,
    int32_t clip_right, int32_t clip_bottom)
{
    struct cp_resource_info info = {
        .struct_size = sizeof(info),
    };
    char id[32];
    const uint16_t *source;
    int32_t destination_width;
    int32_t destination_height;
    int source_handle;
    int32_t y;

    if(command->payload_size == 0 ||
       command->payload_size >= sizeof(id) ||
       memchr(payload, '\0', command->payload_size) != NULL ||
       command->width < 0 || command->height < 0)
        return false;
    memcpy(id, payload, command->payload_size);
    id[command->payload_size] = '\0';
    if(crazypod_miniapps_resource_stat(id, &info) !=
           CRAZYPOD_MINIAPP_OK ||
       (info.type != CP_RESOURCE_BITMAP_RGB565 &&
        info.type != CP_RESOURCE_SPRITE_SHEET) ||
       info.width == 0 || info.height == 0 ||
       info.size != (uint32_t)info.width * info.height * 2u ||
       !crazypod_miniapp_host_memory_reserve(info.size))
        return false;
    source_handle = core_alloc_ex(
        info.size, &buflib_ops_locked);
    if(source_handle <= 0) {
        crazypod_miniapp_host_memory_release(info.size);
        return false;
    }
    source = core_get_data(source_handle);
    if(crazypod_miniapps_resource_read(
           id, 0, (void *)source, info.size) != (int)info.size) {
        core_free(source_handle);
        crazypod_miniapp_host_memory_release(info.size);
        return false;
    }
    destination_width =
        command->width > 0 ? command->width : info.width;
    destination_height =
        command->height > 0 ? command->height : info.height;
    for(y = 0; y < destination_height; ++y) {
        int32_t source_y =
            (int32_t)((int64_t)y * info.height /
                      destination_height);
        int32_t x;

        for(x = 0; x < destination_width; ++x) {
            int32_t source_x =
                (int32_t)((int64_t)x * info.width /
                          destination_width);
            uint16_t color = source[
                (size_t)source_y * info.width + source_x];

            canvas_pixel(
                node, command->x + x, command->y + y,
                color, command->opacity,
                clip_left, clip_top, clip_right, clip_bottom);
        }
    }
    core_free(source_handle);
    crazypod_miniapp_host_memory_release(info.size);
    return true;
}

bool crazypod_miniapp_scene_canvas_commit(
    struct crazypod_miniapp_scene_node *node,
    const void *data, size_t size)
{
    struct cp_canvas_header header;
    const uint8_t *bytes = data;
    int32_t clip_left = 0;
    int32_t clip_top = 0;
    int32_t clip_right;
    int32_t clip_bottom;
    uint16_t index;

    if(node == NULL || data == NULL ||
       size < sizeof(header) || !canvas_allocate(node))
        return false;
    memcpy(&header, data, sizeof(header));
    if(header.magic != CP_CANVAS_MAGIC ||
       header.version != CP_CANVAS_COMMAND_ABI ||
       header.header_size != CP_CANVAS_HEADER_SIZE ||
       header.command_size != CP_CANVAS_COMMAND_SIZE ||
       header.command_count > CP_CANVAS_COMMAND_MAX ||
       header.total_size != size ||
       header.payload_offset !=
           (uint32_t)header.header_size +
               (uint32_t)header.command_count *
                   header.command_size ||
       header.payload_offset > size ||
       header.payload_size !=
           size - header.payload_offset)
        return false;
    clip_right = node->values[CP_UI_PROP_WIDTH];
    clip_bottom = node->values[CP_UI_PROP_HEIGHT];
    for(index = 0; index < header.command_count; ++index) {
        struct cp_canvas_command command;
        const uint8_t *payload;
        uint16_t color;
        int32_t x;
        int32_t y;

        memcpy(
            &command,
            bytes + header.header_size +
                (size_t)index * header.command_size,
            sizeof(command));
        if(command.opcode < CP_CANVAS_CLEAR ||
           command.opcode > CP_CANVAS_IMAGE ||
           !canvas_payload(
               &header, &command, bytes, &payload))
            return false;
        if(command.opcode != CP_CANVAS_TEXT &&
           command.opcode != CP_CANVAS_IMAGE &&
           (command.flags != 0 || command.payload_size != 0))
            return false;
        color = lv_color_to_u16(
            lv_color_hex(command.color & 0xffffffu));
        if(command.opcode == CP_CANVAS_CLEAR) {
            for(y = clip_top; y < clip_bottom; ++y)
                for(x = clip_left; x < clip_right; ++x)
                    canvas_pixel(
                        node, x, y, color, command.opacity,
                        clip_left, clip_top,
                        clip_right, clip_bottom);
        }
        else if(command.opcode == CP_CANVAS_FILL_RECT) {
            int32_t right = command.x + command.width;
            int32_t bottom = command.y + command.height;
            for(y = command.y; y < bottom; ++y)
                for(x = command.x; x < right; ++x)
                    canvas_pixel(
                        node, x, y, color, command.opacity,
                        clip_left, clip_top,
                        clip_right, clip_bottom);
        }
        else if(command.opcode == CP_CANVAS_PIXEL) {
            canvas_pixel(
                node, command.x, command.y,
                color, command.opacity,
                clip_left, clip_top, clip_right, clip_bottom);
        }
        else if(command.opcode == CP_CANVAS_LINE) {
            int32_t x0 = command.x;
            int32_t y0 = command.y;
            int32_t x1 = command.width;
            int32_t y1 = command.height;
            int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1;
            int32_t sx = x0 < x1 ? 1 : -1;
            int32_t dy = y1 > y0 ? y0 - y1 : y1 - y0;
            int32_t sy = y0 < y1 ? 1 : -1;
            int32_t error = dx + dy;

            while(true) {
                int32_t twice;
                canvas_pixel(
                    node, x0, y0, color, command.opacity,
                    clip_left, clip_top,
                    clip_right, clip_bottom);
                if(x0 == x1 && y0 == y1)
                    break;
                twice = 2 * error;
                if(twice >= dy) {
                    error += dy;
                    x0 += sx;
                }
                if(twice <= dx) {
                    error += dx;
                    y0 += sy;
                }
            }
        }
        else if(command.opcode == CP_CANVAS_CLIP) {
            int32_t right = command.x + command.width;
            int32_t bottom = command.y + command.height;

            if(command.width < 0 || command.height < 0)
                return false;
            if(command.x > clip_left)
                clip_left = command.x;
            if(command.y > clip_top)
                clip_top = command.y;
            if(right < clip_right)
                clip_right = right;
            if(bottom < clip_bottom)
                clip_bottom = bottom;
            if(clip_right < clip_left)
                clip_right = clip_left;
            if(clip_bottom < clip_top)
                clip_bottom = clip_top;
        }
        else if(command.opcode == CP_CANVAS_RESET_CLIP) {
            clip_left = 0;
            clip_top = 0;
            clip_right = node->values[CP_UI_PROP_WIDTH];
            clip_bottom = node->values[CP_UI_PROP_HEIGHT];
        }
        else if(command.opcode == CP_CANVAS_TEXT) {
            if(!canvas_draw_text(
                   node, &command, payload,
                   clip_left, clip_top,
                   clip_right, clip_bottom))
                return false;
        }
        else if(!canvas_draw_image(
                    node, &command, payload,
                    clip_left, clip_top,
                    clip_right, clip_bottom)) {
            return false;
        }
    }
    lv_obj_invalidate(node->object);
    return true;
}

void crazypod_miniapp_scene_property_apply(
    struct crazypod_miniapp_scene_node *node,
    uint16_t property)
{
    lv_obj_t *object = node->object;
    int32_t value = node->values[property];

    if(object == NULL)
        return;
    switch(property) {
    case CP_UI_PROP_VISIBLE:
        if(value)
            lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
        break;
    case CP_UI_PROP_DISABLED:
        if(value)
            lv_obj_add_state(object, LV_STATE_DISABLED);
        else
            lv_obj_remove_state(object, LV_STATE_DISABLED);
        break;
    case CP_UI_PROP_FOCUSED:
        if(value)
            lv_obj_add_state(object, LV_STATE_FOCUSED);
        else
            lv_obj_remove_state(object, LV_STATE_FOCUSED);
        break;
    case CP_UI_PROP_FOCUSABLE:
        if(value)
            lv_obj_add_flag(object, LV_OBJ_FLAG_CLICKABLE);
        else
            lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE);
        break;
    case CP_UI_PROP_X:
        lv_obj_set_x(object, value);
        break;
    case CP_UI_PROP_Y:
        lv_obj_set_y(object, value);
        break;
    case CP_UI_PROP_WIDTH:
        lv_obj_set_width(object, value);
        apply_table_widths(node);
        if(node->type == CP_UI_OBJECT_TEXT &&
           node->values[CP_UI_PROP_MARQUEE])
            crazypod_marquee_configure(object, true);
        if(node->type == CP_UI_OBJECT_CANVAS ||
           node->type == CP_UI_OBJECT_TILEMAP)
            (void)canvas_allocate(node);
        break;
    case CP_UI_PROP_HEIGHT:
        lv_obj_set_height(object, value);
        if(node->values[CP_UI_PROP_NUMBER_OF_LINES] > 0)
            apply_number_of_lines(node);
        if(node->type == CP_UI_OBJECT_CANVAS ||
           node->type == CP_UI_OBJECT_TILEMAP)
            (void)canvas_allocate(node);
        break;
    case CP_UI_PROP_MIN_WIDTH:
        lv_obj_set_style_min_width(object, value, 0);
        break;
    case CP_UI_PROP_MIN_HEIGHT:
        lv_obj_set_style_min_height(object, value, 0);
        break;
    case CP_UI_PROP_MAX_WIDTH:
        lv_obj_set_style_max_width(object, value, 0);
        break;
    case CP_UI_PROP_MAX_HEIGHT:
        lv_obj_set_style_max_height(object, value, 0);
        break;
    case CP_UI_PROP_LAYOUT:
        lv_obj_set_layout(
            object,
            value == CP_UI_LAYOUT_FLEX ? LV_LAYOUT_FLEX :
            value == CP_UI_LAYOUT_GRID ? LV_LAYOUT_GRID :
            LV_LAYOUT_NONE);
        if(value == CP_UI_LAYOUT_GRID)
            apply_grid(node);
        break;
    case CP_UI_PROP_FLEX_FLOW:
        lv_obj_set_flex_flow(
            object,
            value == CP_UI_FLEX_COLUMN ? LV_FLEX_FLOW_COLUMN :
            value == CP_UI_FLEX_ROW_WRAP ? LV_FLEX_FLOW_ROW_WRAP :
            value == CP_UI_FLEX_COLUMN_WRAP
                ? LV_FLEX_FLOW_COLUMN_WRAP : LV_FLEX_FLOW_ROW);
        break;
    case CP_UI_PROP_FLEX_GROW:
        lv_obj_set_flex_grow(object, value);
        break;
    case CP_UI_PROP_GRID_COLUMNS:
    case CP_UI_PROP_GRID_ROWS:
        apply_grid(node);
        break;
    case CP_UI_PROP_ALIGN:
    case CP_UI_PROP_JUSTIFY:
        lv_obj_set_flex_align(
            object,
            flex_align(node->values[CP_UI_PROP_JUSTIFY]),
            flex_align(node->values[CP_UI_PROP_ALIGN]),
            LV_FLEX_ALIGN_START);
        break;
    case CP_UI_PROP_PADDING:
        lv_obj_set_style_pad_all(object, value, 0);
        break;
    case CP_UI_PROP_PADDING_LEFT:
        lv_obj_set_style_pad_left(object, value, 0);
        break;
    case CP_UI_PROP_PADDING_RIGHT:
        lv_obj_set_style_pad_right(object, value, 0);
        break;
    case CP_UI_PROP_PADDING_TOP:
        lv_obj_set_style_pad_top(object, value, 0);
        break;
    case CP_UI_PROP_PADDING_BOTTOM:
        lv_obj_set_style_pad_bottom(object, value, 0);
        break;
    case CP_UI_PROP_MARGIN:
        lv_obj_set_style_margin_all(object, value, 0);
        break;
    case CP_UI_PROP_MARGIN_LEFT:
        lv_obj_set_style_margin_left(object, value, 0);
        break;
    case CP_UI_PROP_MARGIN_RIGHT:
        lv_obj_set_style_margin_right(object, value, 0);
        break;
    case CP_UI_PROP_MARGIN_TOP:
        lv_obj_set_style_margin_top(object, value, 0);
        break;
    case CP_UI_PROP_MARGIN_BOTTOM:
        lv_obj_set_style_margin_bottom(object, value, 0);
        break;
    case CP_UI_PROP_BACKGROUND_COLOR:
        lv_obj_set_style_bg_color(object, lv_color_hex(value), 0);
        break;
    case CP_UI_PROP_BACKGROUND_OPACITY:
        lv_obj_set_style_bg_opa(object, value, 0);
        break;
    case CP_UI_PROP_BORDER_COLOR:
        lv_obj_set_style_border_color(object, lv_color_hex(value), 0);
        break;
    case CP_UI_PROP_BORDER_WIDTH:
        lv_obj_set_style_border_width(object, value, 0);
        break;
    case CP_UI_PROP_BORDER_OPACITY:
        lv_obj_set_style_border_opa(object, value, 0);
        break;
    case CP_UI_PROP_RADIUS:
        lv_obj_set_style_radius(object, value, 0);
        break;
    case CP_UI_PROP_OPACITY:
        lv_obj_set_style_opa(object, value, 0);
        break;
    case CP_UI_PROP_SHADOW_COLOR:
        lv_obj_set_style_shadow_color(object, lv_color_hex(value), 0);
        break;
    case CP_UI_PROP_SHADOW_WIDTH:
        lv_obj_set_style_shadow_width(object, value, 0);
        break;
    case CP_UI_PROP_SHADOW_OPACITY:
        lv_obj_set_style_shadow_opa(object, value, 0);
        break;
    case CP_UI_PROP_TEXT:
        apply_text(node);
        break;
    case CP_UI_PROP_TEXT_COLOR:
        lv_obj_set_style_text_color(object, lv_color_hex(value), 0);
        break;
    case CP_UI_PROP_TEXT_ALIGN:
        lv_obj_set_style_text_align(
            object,
            value == CP_UI_TEXT_ALIGN_CENTER ? LV_TEXT_ALIGN_CENTER :
            value == CP_UI_TEXT_ALIGN_RIGHT ? LV_TEXT_ALIGN_RIGHT :
            LV_TEXT_ALIGN_LEFT, 0);
        break;
    case CP_UI_PROP_LETTER_SPACING:
        lv_obj_set_style_text_letter_space(object, value, 0);
        break;
    case CP_UI_PROP_FONT:
        (void)crazypod_miniapp_scene_text_font_apply(node, value);
        break;
    case CP_UI_PROP_FONT_SIZE:
    case CP_UI_PROP_FONT_WEIGHT:
    case CP_UI_PROP_FONT_STYLE:
    case CP_UI_PROP_LINE_HEIGHT:
        /* The compiler emits the complete typography tuple followed by FONT.
         * Applying each partial setter used to create three transient font
         * instances per label and exhausted the host font slots. */
        break;
    case CP_UI_PROP_FONT_SOURCE:
        apply_text_font(
            node, crazypod_miniapp_asset_font(node->font_source));
        break;
    case CP_UI_PROP_NUMBER_OF_LINES:
        apply_number_of_lines(node);
        break;
    case CP_UI_PROP_MARQUEE:
        if(node->type == CP_UI_OBJECT_TEXT) {
            if(value)
                crazypod_marquee_configure(object, true);
            else
                apply_number_of_lines(node);
        }
        break;
    case CP_UI_PROP_IMAGE_SOURCE:
        apply_image_source(node);
        break;
    case CP_UI_PROP_REVISION:
        if(node->type == CP_UI_OBJECT_NOW_PLAYING_ARTWORK)
            (void)crazypod_miniapp_scene_now_playing_artwork_refresh_node(
                node);
        break;
    case CP_UI_PROP_PHASE:
    case CP_UI_PROP_PLAYING:
    case CP_UI_PROP_WAVE_STYLE:
        if(node->type == CP_UI_OBJECT_SOUND_WAVE)
            lv_obj_invalidate(object);
        break;
    case CP_UI_PROP_VALUE:
        if(node->type == CP_UI_OBJECT_PROGRESS)
            lv_bar_set_value(object, value, LV_ANIM_OFF);
        else if(node->type == CP_UI_OBJECT_ARC)
            lv_arc_set_value(object, value);
        else if(node->type == CP_UI_OBJECT_SLIDER)
            lv_slider_set_value(object, value, LV_ANIM_OFF);
        else if(node->type == CP_UI_OBJECT_DROPDOWN)
            lv_dropdown_set_selected(object, (uint32_t)value);
        else if(node->type == CP_UI_OBJECT_ROLLER)
            lv_roller_set_selected(
                object, (uint32_t)value, LV_ANIM_OFF);
        break;
    case CP_UI_PROP_MINIMUM:
    case CP_UI_PROP_MAXIMUM:
        apply_range(node);
        break;
    case CP_UI_PROP_CHECKED:
        if(value)
            lv_obj_add_state(object, LV_STATE_CHECKED);
        else
            lv_obj_remove_state(object, LV_STATE_CHECKED);
        break;
    case CP_UI_PROP_PLACEHOLDER:
        if(node->type == CP_UI_OBJECT_TEXT_INPUT)
            lv_textarea_set_placeholder_text(
                object, node->placeholder);
        break;
    case CP_UI_PROP_SCROLL_X:
        lv_obj_scroll_to_x(object, value, LV_ANIM_OFF);
        break;
    case CP_UI_PROP_SCROLL_Y:
        lv_obj_scroll_to_y(object, value, LV_ANIM_OFF);
        break;
    case CP_UI_PROP_TRANSLATE_X:
        if(transform_target(node) != NULL)
            lv_obj_set_style_translate_x(
                transform_target(node), value, 0);
        break;
    case CP_UI_PROP_TRANSLATE_Y:
        if(transform_target(node) != NULL)
            lv_obj_set_style_translate_y(
                transform_target(node), value, 0);
        break;
    case CP_UI_PROP_SCALE_X:
        if(transform_target(node) != NULL) {
            if(node->type == CP_UI_OBJECT_NOW_PLAYING_ARTWORK)
                node->artwork_transform_dirty = true;
            else
                lv_obj_set_style_transform_scale_x(
                    transform_target(node), value, 0);
        }
        break;
    case CP_UI_PROP_SCALE_Y:
        if(transform_target(node) != NULL) {
            if(node->type == CP_UI_OBJECT_NOW_PLAYING_ARTWORK)
                node->artwork_transform_dirty = true;
            else
                lv_obj_set_style_transform_scale_y(
                    transform_target(node), value, 0);
        }
        break;
    case CP_UI_PROP_ROTATION:
        if(transform_target(node) != NULL) {
            if(node->type == CP_UI_OBJECT_NOW_PLAYING_ARTWORK)
                node->artwork_transform_dirty = true;
            else
                lv_obj_set_style_transform_rotation(
                    transform_target(node), value, 0);
        }
        break;
    case CP_UI_PROP_TRANSFORM_PIVOT_X:
        if(transform_target(node) != NULL) {
            if(node->type == CP_UI_OBJECT_NOW_PLAYING_ARTWORK)
                node->artwork_transform_dirty = true;
            else
                lv_obj_set_style_transform_pivot_x(
                    transform_target(node), value, 0);
        }
        break;
    case CP_UI_PROP_TRANSFORM_PIVOT_Y:
        if(transform_target(node) != NULL) {
            if(node->type == CP_UI_OBJECT_NOW_PLAYING_ARTWORK)
                node->artwork_transform_dirty = true;
            else
                lv_obj_set_style_transform_pivot_y(
                    transform_target(node), value, 0);
        }
        break;
    case CP_UI_PROP_POSITION:
        if(value == CP_UI_POSITION_ABSOLUTE)
            lv_obj_add_flag(object, LV_OBJ_FLAG_IGNORE_LAYOUT);
        else
            lv_obj_remove_flag(object, LV_OBJ_FLAG_IGNORE_LAYOUT);
        break;
    case CP_UI_PROP_OVERFLOW:
        if(value)
            lv_obj_add_flag(object, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        else
            lv_obj_remove_flag(object, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        break;
    case CP_UI_PROP_DATA:
        (void)crazypod_miniapp_scene_data_apply(node);
        break;
    case CP_UI_PROP_SELECTED_COLUMN:
    case CP_UI_PROP_SELECTED_ROW:
        if(node->type == CP_UI_OBJECT_TILE_VIEW)
            lv_tileview_set_tile_by_index(
                object,
                (uint32_t)node->values[
                    CP_UI_PROP_SELECTED_COLUMN],
                (uint32_t)node->values[
                    CP_UI_PROP_SELECTED_ROW],
                LV_ANIM_OFF);
        break;
    default:
        break;
    }
}

static lv_obj_t *create_progress_bar(lv_obj_t *parent)
{
    lv_obj_t *object = lv_bar_create(parent);

    if(object == NULL)
        return NULL;
    /* CrazyPod disables the stock LVGL themes, so a bare bar otherwise has
       a transparent indicator even when its value is non-zero. */
    lv_obj_set_style_bg_color(
        object, lv_color_hex(0x30343b), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(object, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        object, lv_color_hex(0x5b9cff), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(
        object, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    return object;
}

lv_obj_t *crazypod_miniapp_scene_object_create(
    uint8_t type, lv_obj_t *parent, lv_obj_t *root)
{
    (void)root;
    switch(type) {
    case CP_UI_OBJECT_SCREEN:
        return lv_obj_create(parent);
    case CP_UI_OBJECT_VIEW:
    case CP_UI_OBJECT_MODAL:
    case CP_UI_OBJECT_SCROLL_VIEW:
        return lv_obj_create(parent);
    case CP_UI_OBJECT_TEXT:
        return lv_label_create(parent);
    case CP_UI_OBJECT_IMAGE:
        return lv_image_create(parent);
    case CP_UI_OBJECT_BUTTON:
        return lv_button_create(parent);
    case CP_UI_OBJECT_LIST:
        return lv_list_create(parent);
    case CP_UI_OBJECT_PROGRESS:
        return create_progress_bar(parent);
    case CP_UI_OBJECT_ARC:
        return lv_arc_create(parent);
    case CP_UI_OBJECT_SLIDER:
        return lv_slider_create(parent);
    case CP_UI_OBJECT_SWITCH:
        return lv_switch_create(parent);
    case CP_UI_OBJECT_TEXT_INPUT:
        return lv_textarea_create(parent);
    case CP_UI_OBJECT_CANVAS:
    case CP_UI_OBJECT_TILEMAP:
        return lv_canvas_create(parent);
    case CP_UI_OBJECT_ANIMATED_IMAGE:
        return lv_animimg_create(parent);
    case CP_UI_OBJECT_CHART:
        return lv_chart_create(parent);
    case CP_UI_OBJECT_CHECKBOX:
        return lv_checkbox_create(parent);
    case CP_UI_OBJECT_DROPDOWN:
        return lv_dropdown_create(parent);
    case CP_UI_OBJECT_ROLLER:
        return lv_roller_create(parent);
    case CP_UI_OBJECT_TABLE:
        return lv_table_create(parent);
    case CP_UI_OBJECT_TILE_VIEW:
        return lv_tileview_create(parent);
    case CP_UI_OBJECT_IMAGE_BUTTON:
        return lv_imagebutton_create(parent);
    case CP_UI_OBJECT_NOW_PLAYING_ARTWORK:
    case CP_UI_OBJECT_SOUND_WAVE:
        return lv_obj_create(parent);
    default:
        return NULL;
    }
}

void crazypod_miniapp_scene_object_prepare(
    struct crazypod_miniapp_scene_node *node)
{
    if(node == NULL || node->object == NULL)
        return;
    if(node->type == CP_UI_OBJECT_SOUND_WAVE) {
        lv_obj_set_style_bg_opa(node->object, LV_OPA_TRANSP, 0);
        lv_obj_remove_flag(node->object, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(
            node->object, draw_sound_wave,
            LV_EVENT_DRAW_MAIN, node);
    }
    else if(node->type == CP_UI_OBJECT_NOW_PLAYING_ARTWORK) {
        lv_obj_remove_flag(node->object, LV_OBJ_FLAG_CLICKABLE);
    }
}

#endif
