#include "config.h"

#include "../../../crazypod_l10n.h"

#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "lcd.h"

#include "../../../crazypod_books.h"
#include "../../../crazypod_book_image.h"
#include "../../../epub/crazypod_epub_html.h"
#include "../../presentation/crazypod_ui_widgets.h"
#include "crazypod_books_screen.h"

#define CRAZYPOD_BOOKS_IMAGE_MARGIN 10
#define CRAZYPOD_BOOKS_IMAGE_MAX_HEIGHT 204
#define CRAZYPOD_BOOKS_WHITE 0xFFFFFF
#define CRAZYPOD_BOOKS_PANEL 0x1B1B22

static lv_font_t reader_font_12;
static lv_font_t reader_font_14;
static lv_font_t reader_font_16;
static bool reader_fonts_ready;
static char reader_line[2048];
static char reader_sanitized_line[2048];

static void reader_init_fonts(void)
{
    if(reader_fonts_ready)
        return;
    reader_font_14 = lv_font_source_han_sans_sc_14_cjk;
    reader_font_14.fallback = &lv_font_unscii_16;
    reader_font_16 = lv_font_source_han_sans_sc_16_cjk;
    reader_font_16.fallback = &reader_font_14;
    reader_font_12 = lv_font_crazypod_i18n_12;
    reader_font_12.fallback = &reader_font_14;
    reader_fonts_ready = true;
}

unsigned crazypod_books_screen_reader_size(int setting)
{
    if(setting == 0)
        return 12;
    if(setting == 2)
        return 16;
    return 14;
}

const lv_font_t *crazypod_books_screen_reader_font(unsigned size)
{
    /* The reader deliberately uses the bundled faces instead of the
     * optional AOT font pool.  A missing or stale AOT pack must not collapse
     * all three reader choices onto one fallback face.  These fonts contain
     * the CJK repertoire as well as the Latin punctuation used by books, so
     * pagination and rendering remain deterministic on a cold device. */
    reader_init_fonts();
    if(size <= 12)
        return &reader_font_12;
    if(size >= 16)
        return &reader_font_16;
    return &reader_font_14;
}

unsigned crazypod_books_screen_measure_width(
    uint32_t codepoint, uint32_t next_codepoint, void *context)
{
    const lv_font_t *font = context;

    if(font == NULL)
        return 0;
    return lv_font_get_glyph_width(font, codepoint, next_codepoint);
}

static int reader_style_indent_px(const lv_font_t *font, int style)
{
    int space_width;
    int count;

    if(font == NULL)
        return 0;
    style &= 0x7f;
    if(style == CRAZYPOD_EPUB_FORMAT_QUOTE)
        count = 3;
    else if(style == CRAZYPOD_EPUB_FORMAT_NORMAL ||
            style == CRAZYPOD_EPUB_FORMAT_LIST)
        count = 2;
    else
        return 0;
    space_width = lv_font_get_glyph_width(font, ' ', 0);
    return space_width > 0 ? space_width * count : 0;
}

static size_t reader_utf8_decode(
    const unsigned char *data, size_t count, uint32_t *codepoint)
{
    unsigned first;
    uint32_t value;
    size_t bytes;
    size_t i;

    if(data == NULL || count == 0 || codepoint == NULL)
        return 0;
    first = data[0];
    if(first < 0x80) {
        *codepoint = first;
        return 1;
    }
    if(first >= 0xc2 && first <= 0xdf) {
        bytes = 2;
        value = first & 0x1f;
    }
    else if(first >= 0xe0 && first <= 0xef) {
        bytes = 3;
        value = first & 0x0f;
    }
    else if(first >= 0xf0 && first <= 0xf4) {
        bytes = 4;
        value = first & 0x07;
    }
    else {
        *codepoint = 0xfffd;
        return 1;
    }
    if(bytes > count) {
        *codepoint = 0xfffd;
        return 1;
    }
    for(i = 1; i < bytes; ++i) {
        if((data[i] & 0xc0) != 0x80) {
            *codepoint = 0xfffd;
            return 1;
        }
        value = (value << 6) | (data[i] & 0x3f);
    }
    if((bytes == 3 && value < 0x800) ||
       (bytes == 4 && value < 0x10000) ||
       (value >= 0xd800 && value <= 0xdfff) || value > 0x10ffff)
        *codepoint = 0xfffd;
    else
        *codepoint = value;
    return bytes;
}

static size_t reader_sanitize_line(
    const char *text, size_t text_size, const lv_font_t *font)
{
    size_t input = 0;
    size_t output = 0;

    while(input < text_size && output + 1 < sizeof(reader_sanitized_line)) {
        lv_font_glyph_dsc_t glyph;
        uint32_t codepoint;
        size_t bytes = reader_utf8_decode(
            (const unsigned char *)text + input,
            text_size - input, &codepoint);
        bool found;

        if(bytes == 0)
            break;
        if(codepoint == '\n' || codepoint == '\r') {
            if(output + bytes >= sizeof(reader_sanitized_line))
                break;
            memcpy(reader_sanitized_line + output, text + input, bytes);
            output += bytes;
            input += bytes;
            continue;
        }
        found = lv_font_get_glyph_dsc(font, &glyph, codepoint, 0);
        if(found && codepoint != 0) {
            if(output + bytes >= sizeof(reader_sanitized_line))
                break;
            memcpy(reader_sanitized_line + output, text + input, bytes);
            output += bytes;
        }
        else {
            reader_sanitized_line[output++] = '?';
        }
        input += bytes;
    }
    reader_sanitized_line[output] = '\0';
    return output;
}

static void render_reader_line(
    lv_obj_t *page, const char *text, size_t text_size,
    const lv_font_t *font, uint32_t ink_color, int style,
    bool first_line, int *line_y, unsigned line_height)
{
    lv_obj_t *label;
    int base_style = style & 0x7f;
    int indent = reader_style_indent_px(font, style);
    int x = CRAZYPOD_BOOKS_READER_MARGIN +
        ((base_style == CRAZYPOD_EPUB_FORMAT_QUOTE ||
          base_style == CRAZYPOD_EPUB_FORMAT_LIST || first_line)
             ? indent : 0);
    int width = LCD_WIDTH - CRAZYPOD_BOOKS_READER_MARGIN * 2 -
        (x - CRAZYPOD_BOOKS_READER_MARGIN);
    bool centered = (style & CRAZYPOD_EPUB_FORMAT_CENTER) != 0 ||
        base_style == CRAZYPOD_EPUB_FORMAT_HEADING;

    if(width < 1)
        width = 1;
    if(centered) {
        x = CRAZYPOD_BOOKS_READER_MARGIN;
        width = LCD_WIDTH - CRAZYPOD_BOOKS_READER_MARGIN * 2;
    }
    if(text_size >= sizeof(reader_line))
        text_size = sizeof(reader_line) - 1;
    if(text != reader_line)
        memcpy(reader_line, text, text_size);
    reader_line[text_size] = '\0';
    reader_sanitize_line(reader_line, text_size, font);
    label = crazypod_ui_widget_label(
        page, reader_sanitized_line, font, ink_color, LV_OPA_COVER);
    /* The generic label helper may resolve localized UI fonts.  Re-apply the
     * reader face so a book's text can never be promoted to a metadata size.
     */
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_pos(label, x, *line_y);
    lv_obj_set_size(label, width, line_height);
    lv_obj_set_style_pad_all(label, 0, 0);
    lv_obj_set_style_text_line_space(label, 0, 0);
    if(centered)
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    *line_y += (int)line_height;
}

static bool reader_has_format_markers(const char *text)
{
    return text != NULL &&
        strchr(text, CRAZYPOD_EPUB_FORMAT_MARKER) != NULL;
}

static void render_reader_rich_text(
    lv_obj_t *page, const char *text, const lv_font_t *font,
    uint32_t ink_color, int reader_top, int reader_height,
    unsigned line_height)
{
    size_t line_size = 0;
    size_t input = 0;
    int style = CRAZYPOD_EPUB_FORMAT_NORMAL;
    int line_y = reader_top;
    bool first_line = false;

    while(text != NULL) {
        unsigned char value = (unsigned char)text[input];

        if(value == (unsigned char)CRAZYPOD_EPUB_FORMAT_MARKER &&
           text[input + 1] != '\0') {
            if(line_size > 0) {
                if(line_y < reader_top + reader_height)
                    render_reader_line(
                        page, reader_line, line_size, font, ink_color,
                        style, first_line, &line_y, line_height);
                line_size = 0;
                first_line = false;
            }
            style = (unsigned char)text[input + 1];
            first_line = true;
            input += 2;
            continue;
        }
        if(value == '\n' || value == '\0') {
            if(line_y < reader_top + reader_height)
                render_reader_line(
                    page, reader_line, line_size, font, ink_color,
                    style, first_line, &line_y, line_height);
            {
                bool had_text = line_size > 0;

                line_size = 0;
                if(had_text)
                    first_line = false;
            }
            if(value == '\0')
                break;
            ++input;
            continue;
        }
        if(line_size + 1 < sizeof(reader_line))
            reader_line[line_size++] = (char)value;
        ++input;
    }
}

unsigned crazypod_books_screen_reader_line_height(unsigned size)
{
    const lv_font_t *font = crazypod_books_screen_reader_font(size);
    int line_height;

    /* Pagination must follow the font actually rendered.  The bundled
     * fallback fonts have different metrics from the runtime font files. */
    if(font == NULL)
        return size > 0 ? size : 1;
    line_height = lv_font_get_line_height(font);
    return line_height > 0 ? (unsigned)line_height : 1;
}

void crazypod_books_screen_render_reader(
    lv_obj_t *content, int book_index, uint32_t page_offset,
    const char *page_text, uint32_t page_color, uint32_t ink_color,
    bool toolbar_visible)
{
    const struct crazypod_book *book =
        crazypod_book_get(book_index);
    int theme = crazypod_books_theme();
    const lv_font_t *reader_font = crazypod_books_screen_reader_font(
        crazypod_books_screen_reader_size(crazypod_books_font_size()));
    lv_obj_t *page;
    lv_obj_t *toolbar;
    lv_obj_t *label;
    lv_obj_t *image;
    const lv_image_dsc_t *reader_image;
    char image_path[MAX_PATH];
    bool image_requested;
    int reader_height = (toolbar_visible
        ? CRAZYPOD_BOOKS_READER_TOOLBAR_TOP : LCD_HEIGHT) -
        CRAZYPOD_BOOKS_READER_TOP - CRAZYPOD_BOOKS_READER_BOTTOM_MARGIN;
    int image_decode_height = reader_height >
        CRAZYPOD_BOOKS_IMAGE_MAX_HEIGHT
            ? CRAZYPOD_BOOKS_IMAGE_MAX_HEIGHT : reader_height;
    char progress[24];
    uint32_t total = book != NULL && book->content_size > 0
        ? book->content_size : book != NULL ? book->size : 0;
    unsigned percent = total > 0
        ? page_offset * 100u / total : 0;

    page = crazypod_ui_widget_box(content, 0, 0,
                    LCD_WIDTH, LCD_HEIGHT, 0,
                    page_color, LV_OPA_COVER);
    image_requested = crazypod_book_page_image(
        book_index, page_offset, image_path, sizeof(image_path));
    reader_image = crazypod_book_image_get(
        book_index, page_offset,
        LCD_WIDTH - CRAZYPOD_BOOKS_IMAGE_MARGIN * 2,
        image_decode_height);
    if(reader_image != NULL) {
        uint32_t scale_x = (uint32_t)(LCD_WIDTH -
            CRAZYPOD_BOOKS_IMAGE_MARGIN * 2) *
            LV_SCALE_NONE / reader_image->header.w;
        uint32_t scale_y = (uint32_t)reader_height * LV_SCALE_NONE /
            reader_image->header.h;
        uint32_t scale = scale_x < scale_y ? scale_x : scale_y;
        int width;
        int height;

        if(scale > LV_SCALE_NONE)
            scale = LV_SCALE_NONE;
        if(scale == 0)
            scale = 1;
        width = reader_image->header.w * scale / LV_SCALE_NONE;
        height = reader_image->header.h * scale / LV_SCALE_NONE;
        image = lv_image_create(page);
        lv_image_set_src(image, reader_image);
        lv_image_set_pivot(image, 0, 0);
        lv_image_set_scale(image, scale);
        lv_obj_set_pos(
            image, CRAZYPOD_BOOKS_IMAGE_MARGIN +
            ((LCD_WIDTH - CRAZYPOD_BOOKS_IMAGE_MARGIN * 2) - width) / 2,
            CRAZYPOD_BOOKS_READER_TOP +
            (reader_height - height) / 2);
        lv_obj_remove_flag(image, LV_OBJ_FLAG_CLICKABLE);
    }
    else {
        if(image_requested) {
            label = crazypod_ui_widget_label(
                page, CP_TR("Image unavailable"), reader_font,
                ink_color, LV_OPA_COVER);
            lv_obj_set_pos(label, CRAZYPOD_BOOKS_READER_MARGIN,
                           CRAZYPOD_BOOKS_READER_TOP);
            lv_obj_set_size(
                label, LCD_WIDTH - CRAZYPOD_BOOKS_READER_MARGIN * 2,
                reader_height);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        }
        else if(reader_has_format_markers(page_text))
            render_reader_rich_text(
                page, page_text, reader_font, ink_color,
                CRAZYPOD_BOOKS_READER_TOP, reader_height,
                crazypod_books_screen_reader_line_height(
                    crazypod_books_screen_reader_size(
                        crazypod_books_font_size())) +
                    CRAZYPOD_BOOKS_READER_LINE_SPACE);
        else {
            size_t text_size = strlen(page_text);

            if(text_size >= sizeof(reader_line))
                text_size = sizeof(reader_line) - 1;
            reader_sanitize_line(page_text, text_size, reader_font);
            label = crazypod_ui_widget_label(
                page,
                reader_sanitized_line[0] != '\0'
                    ? reader_sanitized_line
                    : page_text[0] != '\0'
                        ? page_text : CP_TR("This book could not be decoded."),
                reader_font, ink_color,
                LV_OPA_COVER);
            lv_obj_set_style_text_font(label, reader_font, 0);
            lv_obj_set_pos(label, CRAZYPOD_BOOKS_READER_MARGIN,
                           CRAZYPOD_BOOKS_READER_TOP);
            lv_obj_set_size(
                label, LCD_WIDTH - CRAZYPOD_BOOKS_READER_MARGIN * 2,
                reader_height);
            lv_obj_set_style_pad_all(label, 0, 0);
            lv_obj_set_style_text_line_space(
                label, CRAZYPOD_BOOKS_READER_LINE_SPACE, 0);
            lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
        }
    }

    if(!toolbar_visible)
        return;
    toolbar = crazypod_ui_widget_box(
        page, 0, CRAZYPOD_BOOKS_READER_TOOLBAR_TOP,
        LCD_WIDTH, LCD_HEIGHT - CRAZYPOD_BOOKS_READER_TOOLBAR_TOP, 0,
        theme == 3 ? 0xFFFFFF : 0x000000,
        theme == 3 ? 24 : 15);
    label = crazypod_ui_widget_label(toolbar, LV_SYMBOL_LEFT,
                       &lv_font_montserrat_16,
                       ink_color, 155);
    lv_obj_set_pos(label, 47, 6);
    label = crazypod_ui_widget_label(
        toolbar,
        LV_SYMBOL_LIST,
        &lv_font_montserrat_12,
        ink_color, 155);
    lv_obj_set_pos(label, 151, 7);
    label = crazypod_ui_widget_label(toolbar, LV_SYMBOL_RIGHT,
                       &lv_font_montserrat_16,
                       ink_color, 155);
    lv_obj_set_pos(label, 255, 6);
    snprintf(progress, sizeof(progress), "%u%%", percent);
    label = crazypod_ui_widget_label(toolbar, progress, &lv_font_montserrat_8,
                       ink_color, 115);
    lv_obj_set_width(label, 42);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 139, 23);
}

void crazypod_books_screen_render_stats(lv_obj_t *content)
{
    lv_obj_t *label;
    lv_obj_t *panel;
    char text[160];

    label = crazypod_ui_widget_label(content, CP_TR("READING STATS"),
                       &lv_font_montserrat_16,
                       CRAZYPOD_BOOKS_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 14, 43);
    panel = crazypod_ui_widget_box(content, 14, 74, 292, 126, 10,
                     CRAZYPOD_BOOKS_PANEL, 220);
    snprintf(text, sizeof(text),
             CP_FMT("%d books\n%d recently opened\n%d favorites\n\n"
                    "Progress is stored on this iPod."),
             crazypod_books_count(),
             crazypod_books_recent_count(),
             crazypod_books_favorite_count());
    label = crazypod_ui_widget_label(
        panel, text, crazypod_books_screen_reader_font(14),
        CRAZYPOD_BOOKS_WHITE, 230);
    lv_obj_set_pos(label, 14, 13);
    lv_obj_set_width(label, 264);
}

void crazypod_books_screen_render_info(lv_obj_t *content,
                                       int book_index)
{
    const struct crazypod_book *book;
    lv_obj_t *label;
    lv_obj_t *panel;
    char text[256];
    const char *format;

    crazypod_book_probe(book_index);
    book = crazypod_book_get(book_index);
    format = book == NULL ? "" :
        book->format == CRAZYPOD_BOOK_TXT ? CP_TR("TXT") :
        book->format == CRAZYPOD_BOOK_MARKDOWN ? CP_TR("Markdown") : CP_TR("EPUB");

    label = crazypod_ui_widget_label(content, CP_TR("BOOK INFO"),
                       &lv_font_montserrat_16,
                       CRAZYPOD_BOOKS_WHITE, LV_OPA_COVER);
    lv_obj_set_pos(label, 14, 43);
    panel = crazypod_ui_widget_box(content, 14, 74, 292, 132, 10,
                     CRAZYPOD_BOOKS_PANEL, 220);
    snprintf(text, sizeof(text),
             CP_FMT("%.60s\n%s%.60s%s\n%.8s · %lu KB\n\n%.80s"),
             book != NULL ? book->title : CP_FMT("Missing Book"),
             book != NULL && book->author[0] != '\0' ? CP_FMT("by ") : "",
             book != NULL ? book->author : "",
             book != NULL && book->author[0] != '\0' ? "" : CP_FMT("Unknown author"),
             format,
             (unsigned long)(book != NULL ? book->size / 1024u : 0),
             book != NULL ? book->path : "");
    label = crazypod_ui_widget_label(
        panel, text, crazypod_books_screen_reader_font(14),
        CRAZYPOD_BOOKS_WHITE, 225);
    lv_obj_set_pos(label, 12, 10);
    lv_obj_set_width(label, 268);
    lv_obj_set_height(label, 112);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
}
