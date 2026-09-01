#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "file.h"
#include "font.h"
#include "kernel.h"
#include "rbpaths.h"
#include "rbunicode.h"
#include "string-extra.h"
#include "lvgl.h"
#include "crazypod_l10n.h"
#include "crazypod_runtime_font.h"

#define CRAZYPOD_FONT_MIN_SIZE 6
#define CRAZYPOD_FONT_MAX_SIZE 48
/* Shell faces and a certified theme's faces/line-height variants coexist. */
#define CRAZYPOD_RUNTIME_FONT_MAX 48
#define CRAZYPOD_ASSET_FONT_MAX 4
#define CRAZYPOD_FONT_COVERAGE_BYTES 8192
#define CRAZYPOD_FONT_HEADER_SIZE 36
#define CRAZYPOD_FONT_LONG_OFFSET_THRESHOLD 0xffdb

struct crazypod_asset_font_slot {
    char key[32];
    char path[MAX_PATH];
    int font_id;
    uint8_t *coverage;
    lv_font_t lv_font;
    unsigned requested_line_height;
    bool semantic;
    bool persistent;
    bool used;
    struct mutex glyph_mutex;
};

static struct crazypod_asset_font_slot runtime_fonts[
    CRAZYPOD_RUNTIME_FONT_MAX];
static struct crazypod_asset_font_slot asset_fonts[
    CRAZYPOD_ASSET_FONT_MAX];
static uint8_t runtime_font_coverage[
    CRAZYPOD_RUNTIME_FONT_MAX][CRAZYPOD_FONT_COVERAGE_BYTES];
static uint8_t asset_font_coverage[
    CRAZYPOD_ASSET_FONT_MAX][CRAZYPOD_FONT_COVERAGE_BYTES];
static bool initialized;
static char last_error[MAX_PATH + 64];

static bool asset_get_glyph_dsc(
    const lv_font_t *font, lv_font_glyph_dsc_t *glyph,
    uint32_t letter, uint32_t letter_next);
static const void *asset_get_glyph_bitmap(
    lv_font_glyph_dsc_t *glyph, lv_draw_buf_t *draw_buffer);
static bool asset_ensure_loaded(
    struct crazypod_asset_font_slot *slot);
static bool semantic_slot_use_path(
    struct crazypod_asset_font_slot *slot, const char *path);
static const lv_font_t *semantic_font_resolve(
    enum crazypod_font_family family, unsigned size, unsigned weight,
    enum crazypod_font_style style, unsigned line_height, bool persistent);
static const lv_font_t *fallback_font_resolve(
    unsigned size, unsigned line_height, bool persistent);

static void record_error(
    const char *reason, const char *family, unsigned size,
    unsigned weight, unsigned line_height, const char *path)
{
    if(last_error[0] != '\0')
        return;
    snprintf(
        last_error, sizeof(last_error),
        "%s family=%s size=%u weight=%u lineHeight=%u path=%s",
        reason, family != NULL ? family : "invalid",
        size, weight, line_height, path != NULL ? path : "-");
}

void crazypod_runtime_font_error_clear(void)
{
    last_error[0] = '\0';
}

const char *crazypod_runtime_font_last_error(void)
{
    return last_error;
}

static const char *locale_name(void)
{
    switch(crazypod_language_current()) {
    case CRAZYPOD_LANGUAGE_KOREAN:
        return "kr";
    case CRAZYPOD_LANGUAGE_CHINESE_SIMPLIFIED:
        return "sc";
    case CRAZYPOD_LANGUAGE_CHINESE_TRADITIONAL:
        return "tc";
    case CRAZYPOD_LANGUAGE_JAPANESE:
        return "jp";
    default:
        /* The JP Noto face also contains Latin, Greek, Cyrillic, Hangul and
         * the shared CJK repertoire. */
        return "jp";
    }
}

static const char *weight_name(unsigned weight)
{
    switch(weight) {
    case 100: return "Thin";
    case 200: return "ExtraLight";
    case 300: return "Light";
    case 400: return "Regular";
    case 500: return "Medium";
    case 600: return "SemiBold";
    case 700: return "Bold";
    case 800: return "ExtraBold";
    case 900: return "Black";
    default: return NULL;
    }
}

static const char *family_name(enum crazypod_font_family family)
{
    switch(family) {
    case CRAZYPOD_FONT_FAMILY_SYSTEM: return "system";
    case CRAZYPOD_FONT_FAMILY_SERIF: return "serif";
    case CRAZYPOD_FONT_FAMILY_MONO: return "mono";
    default: return NULL;
    }
}

/* Keep system text layout stable when Simplified Chinese switches from
 * Noto's native metrics to PingFang SC's native metrics. */
static unsigned system_line_height(unsigned size)
{
    return (size * 1448u + 999u) / 1000u;
}

bool crazypod_runtime_font_init(void)
{
    if(initialized)
        return true;
    initialized = true;
    return crazypod_runtime_font_at_size(12) != NULL &&
        crazypod_runtime_font_at_size(15) != NULL &&
        crazypod_runtime_font_at_size(18) != NULL;
}

const lv_font_t *crazypod_runtime_font_resolve(
    enum crazypod_font_family family, unsigned size, unsigned weight,
    enum crazypod_font_style style, unsigned line_height)
{
    initialized = true;
    return semantic_font_resolve(
        family, size, weight, style, line_height, false);
}

const lv_font_t *crazypod_runtime_font(void)
{
    return semantic_font_resolve(
        CRAZYPOD_FONT_FAMILY_SYSTEM, 16, 400,
        CRAZYPOD_FONT_STYLE_NORMAL, 0, true);
}

const lv_font_t *crazypod_runtime_font_at_size(unsigned size)
{
    return crazypod_runtime_font_at_size_weight(size, 400);
}

const lv_font_t *crazypod_runtime_font_at_size_weight(
    unsigned size, unsigned weight)
{
    return semantic_font_resolve(
        CRAZYPOD_FONT_FAMILY_SYSTEM, size, weight,
        CRAZYPOD_FONT_STYLE_NORMAL, 0, true);
}

void crazypod_runtime_font_prewarm_text(
    unsigned size, const char *text)
{
    const lv_font_t *lv_font;
    struct crazypod_asset_font_slot *slot;
    struct font *font;
    const unsigned char *cursor;
    ucschar_t character;

    if(text == NULL || text[0] == '\0')
        return;
    lv_font = crazypod_runtime_font_at_size(size);
    if(lv_font == NULL)
        return;
    slot = (struct crazypod_asset_font_slot *)lv_font->dsc;
    if(slot == NULL)
        return;
    cursor = (const unsigned char *)text;
    while(*cursor != '\0') {
        cursor = utf8decode(cursor, &character);
        if(character == 0)
            break;
        mutex_lock(&slot->glyph_mutex);
        if(!asset_ensure_loaded(slot)) {
            mutex_unlock(&slot->glyph_mutex);
            return;
        }
        font_lock(slot->font_id, true);
        font = font_get(slot->font_id);
        if(font != NULL)
            (void)font_get_bits(font, character);
        font_lock(slot->font_id, false);
        mutex_unlock(&slot->glyph_mutex);
    }
}

bool crazypod_runtime_fonts_ready(void)
{
    return crazypod_runtime_font_resolve(
               CRAZYPOD_FONT_FAMILY_SYSTEM, 12, 400,
               CRAZYPOD_FONT_STYLE_NORMAL, 0) != NULL &&
        crazypod_runtime_font_resolve(
               CRAZYPOD_FONT_FAMILY_SYSTEM, 15, 400,
               CRAZYPOD_FONT_STYLE_NORMAL, 0) != NULL &&
        crazypod_runtime_font_resolve(
               CRAZYPOD_FONT_FAMILY_SYSTEM, 18, 400,
               CRAZYPOD_FONT_STYLE_NORMAL, 0) != NULL &&
        crazypod_runtime_font_resolve(
               CRAZYPOD_FONT_FAMILY_SERIF, 12, 400,
               CRAZYPOD_FONT_STYLE_NORMAL, 0) != NULL &&
        crazypod_runtime_font_resolve(
               CRAZYPOD_FONT_FAMILY_MONO, 12, 400,
               CRAZYPOD_FONT_STYLE_NORMAL, 0) != NULL;
}

static uint16_t read_le16(const uint8_t *value)
{
    return (uint16_t)value[0] | ((uint16_t)value[1] << 8);
}

static uint32_t read_le32(const uint8_t *value)
{
    return (uint32_t)value[0] | ((uint32_t)value[1] << 8) |
           ((uint32_t)value[2] << 16) | ((uint32_t)value[3] << 24);
}

static bool read_at_exact(
    int fd, uint32_t offset, void *buffer, size_t size)
{
    uint8_t *cursor = buffer;

    if(lseek(fd, (off_t)offset, SEEK_SET) < 0)
        return false;
    while(size > 0) {
        ssize_t count = read(fd, cursor, size);

        if(count <= 0)
            return false;
        cursor += count;
        size -= (size_t)count;
    }
    return true;
}

static bool load_font_coverage(
    const char *path,
    uint8_t coverage[CRAZYPOD_FONT_COVERAGE_BYTES])
{
    uint8_t header[CRAZYPOD_FONT_HEADER_SIZE];
    uint8_t offsets_buffer[256];
    uint8_t default_buffer[4];
    uint32_t first;
    uint32_t default_character;
    uint32_t size;
    uint32_t bits_size;
    uint32_t offset_count;
    uint32_t width_count;
    uint32_t table_offset;
    uint32_t default_offset;
    uint32_t index;
    unsigned offset_size;
    int fd = open(path, O_RDONLY);
    int file_size;

    if(fd < 0)
        return false;
    file_size = filesize(fd);
    if(file_size < CRAZYPOD_FONT_HEADER_SIZE ||
       !read_at_exact(fd, 0, header, sizeof(header)) ||
       memcmp(header, "RB12", 4) != 0 ||
       read_le16(header + 4) == 0 || read_le16(header + 4) > 128 ||
       read_le16(header + 6) == 0 || read_le16(header + 6) > 64 ||
       read_le16(header + 8) > read_le16(header + 6) ||
       read_le16(header + 10) > 1) {
        close(fd);
        return false;
    }
    first = read_le32(header + 12);
    default_character = read_le32(header + 16);
    size = read_le32(header + 20);
    bits_size = read_le32(header + 24);
    offset_count = read_le32(header + 28);
    width_count = read_le32(header + 32);
    offset_size = bits_size < CRAZYPOD_FONT_LONG_OFFSET_THRESHOLD ? 2 : 4;
    table_offset = (CRAZYPOD_FONT_HEADER_SIZE + bits_size +
                    offset_size - 1u) & ~(offset_size - 1u);
    if(size == 0 || first > UINT16_MAX || size > 0x10000u - first ||
       default_character < first || default_character >= first + size ||
       bits_size == 0 ||
       (offset_count != 0 && offset_count != size) ||
       (width_count != 0 && width_count != size) ||
       (uint64_t)table_offset + (uint64_t)offset_count * offset_size +
           width_count != (uint32_t)file_size) {
        close(fd);
        return false;
    }
    memset(coverage, 0, CRAZYPOD_FONT_COVERAGE_BYTES);
    if(offset_count == 0) {
        for(index = 0; index < size; ++index) {
            uint32_t codepoint = first + index;
            coverage[codepoint >> 3] |= 1u << (codepoint & 7);
        }
        close(fd);
        return true;
    }
    if(!read_at_exact(
           fd, table_offset +
               (default_character - first) * offset_size,
           default_buffer, offset_size)) {
        close(fd);
        return false;
    }
    default_offset = offset_size == 2
        ? read_le16(default_buffer) : read_le32(default_buffer);
    for(index = 0; index < size;) {
        uint32_t remaining = size - index;
        uint32_t count = remaining > sizeof(offsets_buffer) / offset_size
            ? sizeof(offsets_buffer) / offset_size : remaining;
        uint32_t cursor;

        if(!read_at_exact(fd, table_offset + index * offset_size,
                          offsets_buffer, count * offset_size)) {
            close(fd);
            return false;
        }
        for(cursor = 0; cursor < count; ++cursor) {
            uint32_t codepoint = first + index + cursor;
            const uint8_t *encoded = offsets_buffer + cursor * offset_size;
            uint32_t offset = offset_size == 2
                ? read_le16(encoded) : read_le32(encoded);

            if(offset >= bits_size) {
                close(fd);
                return false;
            }
            if(codepoint == default_character || offset != default_offset)
                coverage[codepoint >> 3] |= 1u << (codepoint & 7);
        }
        index += count;
    }
    close(fd);
    return true;
}

static void font_slot_initialize(
    struct crazypod_asset_font_slot *slot,
    const char *key, const char *path,
    unsigned line_height, bool semantic, bool persistent)
{
    memset(slot, 0, sizeof(*slot));
    strlcpy(slot->key, key, sizeof(slot->key));
    strlcpy(slot->path, path, sizeof(slot->path));
    slot->font_id = -1;
    slot->requested_line_height = line_height;
    slot->semantic = semantic;
    slot->persistent = persistent;
    slot->lv_font.get_glyph_dsc = asset_get_glyph_dsc;
    slot->lv_font.get_glyph_bitmap = asset_get_glyph_bitmap;
    slot->lv_font.dsc = slot;
    slot->lv_font.subpx = LV_FONT_SUBPX_NONE;
    slot->lv_font.kerning = LV_FONT_KERNING_NONE;
    mutex_init(&slot->glyph_mutex);
    slot->used = true;
}

static bool asset_ensure_loaded(struct crazypod_asset_font_slot *slot)
{
    struct font *font;
    int32_t extra;

    if(slot->font_id >= 0)
        return true;
    slot->font_id = font_load_ex(slot->path, 0, 256);
    if(slot->font_id < 0)
        return false;
    font_lock(slot->font_id, true);
    font = font_get(slot->font_id);
    if(font == NULL || font->depth > 1) {
        font_lock(slot->font_id, false);
        font_unload(slot->font_id);
        slot->font_id = -1;
        return false;
    }
    slot->lv_font.line_height = slot->requested_line_height != 0
        ? (int32_t)slot->requested_line_height : (int32_t)font->height;
    slot->lv_font.base_line = font->height - font->ascent;
    extra = slot->lv_font.line_height - font->height;
    if(extra > 0)
        slot->lv_font.base_line += extra / 2;
    if(!slot->semantic)
        slot->lv_font.fallback = crazypod_runtime_font_resolve(
            CRAZYPOD_FONT_FAMILY_SYSTEM, font->height, 400,
            CRAZYPOD_FONT_STYLE_NORMAL, 0);
    font_lock(slot->font_id, false);
    return true;
}

static bool semantic_slot_use_path(
    struct crazypod_asset_font_slot *slot, const char *path)
{
    unsigned index = (unsigned)(slot - runtime_fonts);
    bool path_changed;
    bool loaded;

    mutex_lock(&slot->glyph_mutex);
    path_changed = strcmp(slot->path, path) != 0;
    if(path_changed) {
        if(slot->font_id >= 0)
            font_unload(slot->font_id);
        slot->font_id = -1;
        strlcpy(slot->path, path, sizeof(slot->path));
        slot->coverage = NULL;
    }
    if(slot->coverage == NULL &&
       (index >= CRAZYPOD_RUNTIME_FONT_MAX ||
        !load_font_coverage(slot->path, runtime_font_coverage[index]))) {
        mutex_unlock(&slot->glyph_mutex);
        return false;
    }
    slot->coverage = runtime_font_coverage[index];
    loaded = asset_ensure_loaded(slot);
    mutex_unlock(&slot->glyph_mutex);
    return loaded;
}

static const lv_font_t *fallback_font_resolve(
    unsigned size, unsigned line_height, bool persistent)
{
    char key[sizeof(runtime_fonts[0].key)];
    char path[sizeof(runtime_fonts[0].path)];
    struct crazypod_asset_font_slot *available = NULL;
    unsigned index;
    int count;

    count = snprintf(
        key, sizeof(key), "fallback-system-400-%u-%u",
        size, line_height);
    if(count < 0 || (size_t)count >= sizeof(key))
        return NULL;
    count = snprintf(
        path, sizeof(path),
        FONT_DIR "/crazypod-aot/jp-system-400-%u.fnt", size);
    if(count < 0 || (size_t)count >= sizeof(path))
        return NULL;
    for(index = 0; index < CRAZYPOD_RUNTIME_FONT_MAX; ++index) {
        struct crazypod_asset_font_slot *slot = &runtime_fonts[index];

        if(slot->used && strcmp(slot->key, key) == 0) {
            if(persistent)
                slot->persistent = true;
            if(!semantic_slot_use_path(slot, path))
                return NULL;
            return &slot->lv_font;
        }
        if(!slot->used && available == NULL)
            available = slot;
    }
    if(available == NULL)
        return NULL;
    font_slot_initialize(
        available, key, path, line_height, true, persistent);
    if(!semantic_slot_use_path(available, path)) {
        memset(available, 0, sizeof(*available));
        return NULL;
    }
    return &available->lv_font;
}

static const lv_font_t *semantic_font_resolve(
    enum crazypod_font_family family, unsigned size, unsigned weight,
    enum crazypod_font_style style, unsigned line_height, bool persistent)
{
    char key[sizeof(runtime_fonts[0].key)];
    char path[sizeof(runtime_fonts[0].path)];
    struct crazypod_asset_font_slot *available = NULL;
    const char *locale = locale_name();
    const char *family_value = family_name(family);
    unsigned index;
    int count;

    if(family_value == NULL || size < CRAZYPOD_FONT_MIN_SIZE ||
       size > CRAZYPOD_FONT_MAX_SIZE || weight_name(weight) == NULL ||
       style != CRAZYPOD_FONT_STYLE_NORMAL ||
       (line_height != 0 && line_height < size)) {
        record_error(
            "invalid font request", family_value, size,
            weight, line_height, NULL);
        return NULL;
    }
    if(line_height == 0 && family == CRAZYPOD_FONT_FAMILY_SYSTEM)
        line_height = system_line_height(size);
    count = snprintf(
        key, sizeof(key), "%s-%u-%u-%u",
        family_value, weight, size, line_height);
    if(count < 0 || (size_t)count >= sizeof(key))
        return NULL;
    count = snprintf(
        path, sizeof(path), FONT_DIR "/crazypod-aot/%s-%s-%u-%u.fnt",
        locale, family_value, weight, size);
    if(count < 0 || (size_t)count >= sizeof(path))
        return NULL;
    for(index = 0; index < CRAZYPOD_RUNTIME_FONT_MAX; ++index) {
        struct crazypod_asset_font_slot *slot = &runtime_fonts[index];

        if(slot->used && strcmp(slot->key, key) == 0) {
            if(persistent)
                slot->persistent = true;
            /* Regional faces have identical metrics. Reload the slot in
             * place so existing LVGL font pointers remain valid while a
             * language change reuses the same semantic cache entry. */
            if(!semantic_slot_use_path(slot, path)) {
                record_error(
                    "font load failed", family_value, size,
                    weight, line_height, path);
                return NULL;
            }
            if(family == CRAZYPOD_FONT_FAMILY_SYSTEM &&
               strcmp(locale, "sc") == 0) {
                slot->lv_font.fallback = fallback_font_resolve(
                    size, line_height, persistent);
                if(slot->lv_font.fallback == NULL) {
                    record_error(
                        "font fallback load failed", family_value, size,
                        weight, line_height, path);
                    return NULL;
                }
            }
            else
                slot->lv_font.fallback = NULL;
            return &slot->lv_font;
        }
        if(!slot->used && available == NULL)
            available = slot;
    }
    if(available == NULL) {
        record_error(
            "font slots exhausted", family_value, size,
            weight, line_height, NULL);
        return NULL;
    }
    font_slot_initialize(
        available, key, path, line_height, true, persistent);
    if(!semantic_slot_use_path(available, path)) {
        record_error(
            "font load failed", family_value, size,
            weight, line_height, path);
        memset(available, 0, sizeof(*available));
        return NULL;
    }
    if(family == CRAZYPOD_FONT_FAMILY_SYSTEM &&
       strcmp(locale, "sc") == 0) {
        available->lv_font.fallback = fallback_font_resolve(
            size, line_height, persistent);
        if(available->lv_font.fallback == NULL) {
            record_error(
                "font fallback load failed", family_value, size,
                weight, line_height, path);
            memset(available, 0, sizeof(*available));
            return NULL;
        }
    }
    return &available->lv_font;
}

static bool asset_get_glyph_dsc(
    const lv_font_t *font, lv_font_glyph_dsc_t *glyph,
    uint32_t letter, uint32_t letter_next)
{
    struct crazypod_asset_font_slot *slot =
        (struct crazypod_asset_font_slot *)font->dsc;
    struct font *rockbox_font;
    int height;
    int width;

    (void)letter_next;
    if(slot == NULL || letter > UINT16_MAX ||
       (slot->coverage != NULL &&
        (slot->coverage[letter >> 3] & (1u << (letter & 7))) == 0))
        return false;
    mutex_lock(&slot->glyph_mutex);
    if(!asset_ensure_loaded(slot)) {
        mutex_unlock(&slot->glyph_mutex);
        return false;
    }
    font_lock(slot->font_id, true);
    rockbox_font = font_get(slot->font_id);
    if(rockbox_font == NULL || letter < rockbox_font->firstchar ||
       letter - rockbox_font->firstchar >= (uint32_t)rockbox_font->size) {
        font_lock(slot->font_id, false);
        mutex_unlock(&slot->glyph_mutex);
        return false;
    }
    width = font_get_width(rockbox_font, letter);
    height = rockbox_font->height;
    font_lock(slot->font_id, false);
    mutex_unlock(&slot->glyph_mutex);
    memset(glyph, 0, sizeof(*glyph));
    glyph->adv_w = width;
    glyph->box_w = width;
    glyph->box_h = height;
    glyph->format = LV_FONT_GLYPH_FORMAT_A8;
    glyph->gid.index = letter;
    return true;
}

static const void *asset_get_glyph_bitmap(
    lv_font_glyph_dsc_t *glyph, lv_draw_buf_t *draw_buffer)
{
    struct crazypod_asset_font_slot *slot;
    struct font *font;
    const unsigned char *source;
    uint8_t *destination;
    unsigned width;
    unsigned height;
    unsigned x;
    unsigned y;

    if(glyph->resolved_font == NULL || draw_buffer == NULL)
        return NULL;
    slot = (struct crazypod_asset_font_slot *)
        glyph->resolved_font->dsc;
    if(slot == NULL)
        return NULL;
    mutex_lock(&slot->glyph_mutex);
    if(!asset_ensure_loaded(slot)) {
        mutex_unlock(&slot->glyph_mutex);
        return NULL;
    }
    font_lock(slot->font_id, true);
    font = font_get(slot->font_id);
    source = font != NULL ? font_get_bits(font, glyph->gid.index) : NULL;
    if(source == NULL) {
        font_lock(slot->font_id, false);
        mutex_unlock(&slot->glyph_mutex);
        return NULL;
    }
    width = glyph->box_w;
    height = glyph->box_h;
    destination = draw_buffer->data;
    memset(destination, 0, draw_buffer->header.stride * height);
    if(font->depth == 0) {
        for(y = 0; y < height; ++y) {
            const unsigned char *source_row = source + (y >> 3) * width;
            uint8_t *row = destination + y * draw_buffer->header.stride;
            unsigned mask = 1u << (y & 7);

            for(x = 0; x < width; ++x)
                row[x] = (source_row[x] & mask) != 0 ? 0xff : 0x00;
        }
    }
    else {
        for(y = 0; y < height; ++y) {
            uint8_t *row = destination + y * draw_buffer->header.stride;

            for(x = 0; x < width; ++x) {
                unsigned pixel = y * width + x;
                unsigned packed = source[pixel >> 1];
                unsigned alpha = (pixel & 1) != 0
                    ? packed >> 4 : packed & 0x0f;
                row[x] = (uint8_t)(0xff - alpha * 0x11);
            }
        }
    }
    font_lock(slot->font_id, false);
    mutex_unlock(&slot->glyph_mutex);
    return draw_buffer;
}

const lv_font_t *crazypod_runtime_asset_font(
    const char *key, const char *path)
{
    size_t key_length;
    size_t path_length;
    unsigned index;

    if(key == NULL || path == NULL)
        return NULL;
    key_length = strlen(key);
    path_length = strlen(path);
    if(key_length == 0 || key_length >= sizeof(asset_fonts[0].key) ||
       path_length == 0 || path_length >= sizeof(asset_fonts[0].path))
        return NULL;
    for(index = 0; index < CRAZYPOD_ASSET_FONT_MAX; ++index) {
        if(asset_fonts[index].used &&
           strcmp(asset_fonts[index].key, key) == 0)
            return &asset_fonts[index].lv_font;
    }
    for(index = 0; index < CRAZYPOD_ASSET_FONT_MAX; ++index) {
        struct crazypod_asset_font_slot *slot = &asset_fonts[index];

        if(slot->used)
            continue;
        if(!load_font_coverage(path, asset_font_coverage[index]))
            return NULL;
        font_slot_initialize(slot, key, path, 0, false, false);
        slot->coverage = asset_font_coverage[index];
        if(!asset_ensure_loaded(slot)) {
            memset(slot, 0, sizeof(*slot));
            return NULL;
        }
        return &slot->lv_font;
    }
    return NULL;
}

void crazypod_runtime_asset_fonts_reset(void)
{
    unsigned index;

    for(index = 0; index < CRAZYPOD_ASSET_FONT_MAX; ++index) {
        if(asset_fonts[index].used && asset_fonts[index].font_id >= 0)
            font_unload(asset_fonts[index].font_id);
        memset(&asset_fonts[index], 0, sizeof(asset_fonts[index]));
    }
    for(index = 0; index < CRAZYPOD_RUNTIME_FONT_MAX; ++index) {
        if(!runtime_fonts[index].used || runtime_fonts[index].persistent)
            continue;
        if(runtime_fonts[index].font_id >= 0)
            font_unload(runtime_fonts[index].font_id);
        memset(&runtime_fonts[index], 0, sizeof(runtime_fonts[index]));
    }
}

#endif
