#include "config.h"

#include "crazypod_l10n.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lcd.h"
#include "system.h"

#include "crazypod_boot_logo.h"
#include "lvgl.h"

#include "crazypod_lcd.h"

static fb_data crazypod_framebuffer[LCD_FBHEIGHT][LCD_FBWIDTH]
    IRAM_LCDFRAMEBUFFER CACHEALIGN_AT_LEAST_ATTR(16);
static lv_image_dsc_t boot_logo_descriptor;

static void *framebuffer_address(int x, int y)
{
    return &crazypod_framebuffer[y][x];
}

struct frame_buffer_t lcd_framebuffer_default = {
    .fb_ptr = &crazypod_framebuffer[0][0],
    .get_address_fn = framebuffer_address,
    .stride = LCD_WIDTH,
    .elems = LCD_WIDTH * LCD_HEIGHT,
};

static struct viewport crazypod_viewport = {
    .x = 0,
    .y = 0,
    .width = LCD_WIDTH,
    .height = LCD_HEIGHT,
    .flags = 0,
    .font = 0,
    .drawmode = DRMODE_SOLID,
    .buffer = &lcd_framebuffer_default,
    .fg_pattern = LCD_WHITE,
    .bg_pattern = LCD_BLACK,
};

struct viewport *lcd_current_viewport = &crazypod_viewport;

void lcd_set_foreground(unsigned color)
{
    lcd_current_viewport->fg_pattern = color;
}

unsigned lcd_get_foreground(void)
{
    return lcd_current_viewport->fg_pattern;
}

void lcd_set_background(unsigned color)
{
    lcd_current_viewport->bg_pattern = color;
}

unsigned lcd_get_background(void)
{
    return lcd_current_viewport->bg_pattern;
}

void lcd_fillrect(int x, int y, int width, int height)
{
    int left = x;
    int top = y;
    int right = x + width;
    int bottom = y + height;
    int row;

    if(left < lcd_current_viewport->x)
        left = lcd_current_viewport->x;
    if(top < lcd_current_viewport->y)
        top = lcd_current_viewport->y;
    if(right > lcd_current_viewport->x + lcd_current_viewport->width)
        right = lcd_current_viewport->x + lcd_current_viewport->width;
    if(bottom > lcd_current_viewport->y + lcd_current_viewport->height)
        bottom = lcd_current_viewport->y + lcd_current_viewport->height;
    if(left >= right || top >= bottom)
        return;

    for(row = top; row < bottom; ++row) {
        fb_data *pixel = &crazypod_framebuffer[row][left];
        fb_data *end = &crazypod_framebuffer[row][right];

        while(pixel < end)
            *pixel++ = lcd_current_viewport->fg_pattern;
    }
}

#ifdef SIMULATOR
static int clamp_video_component(int value)
{
    if(value < 0)
        return 0;
    if(value > 255)
        return 255;
    return value;
}

static bool blit_yuv_to_framebuffer(
    unsigned char * const source[3],
    int source_x, int source_y, int stride,
    int destination_x, int destination_y,
    int width, int height)
{
    int row;
    int column;

    if(destination_x < 0 || destination_y < 0 ||
       destination_x + width > LCD_WIDTH ||
       destination_y + height > LCD_HEIGHT)
        return false;

    width &= ~1;
    height &= ~1;
    for(row = 0; row < height; ++row) {
        int source_row = source_y + row;
        const uint8_t *luma =
            source[0] + source_row * stride + source_x;
        const uint8_t *chroma_u =
            source[1] + (source_row >> 1) * (stride >> 1) +
            (source_x >> 1);
        const uint8_t *chroma_v =
            source[2] + (source_row >> 1) * (stride >> 1) +
            (source_x >> 1);
        fb_data *output =
            &crazypod_framebuffer[destination_y + row][destination_x];

        for(column = 0; column < width; ++column) {
            int y = (int)luma[column] - 16;
            int u = (int)chroma_u[column >> 1] - 128;
            int v = (int)chroma_v[column >> 1] - 128;
            int red;
            int green;
            int blue;

            if(y < 0)
                y = 0;
            red = (298 * y + 409 * v + 128) >> 8;
            green = (298 * y - 100 * u - 208 * v + 128) >> 8;
            blue = (298 * y + 516 * u + 128) >> 8;
            output[column] = LCD_RGBPACK(
                clamp_video_component(red),
                clamp_video_component(green),
                clamp_video_component(blue));
        }
    }
    return true;
}

void lcd_blit_yuv(unsigned char * const source[3],
                  int source_x, int source_y, int stride,
                  int destination_x, int destination_y,
                  int width, int height)
{
    if(!blit_yuv_to_framebuffer(
           source, source_x, source_y, stride,
           destination_x, destination_y, width, height))
        return;
    lcd_update_rect(destination_x, destination_y, width, height);
}
#else
static uint16_t video_line_buffer[LCD_WIDTH * 2]
    CACHEALIGN_AT_LEAST_ATTR(16);

extern void lcd_write_yuv420_lines(
    unsigned char const * const source[3],
    uint16_t *output, int width, int stride);

static bool blit_yuv_to_framebuffer(
    unsigned char * const source[3],
    int source_x, int source_y, int stride,
    int destination_x, int destination_y,
    int width, int height)
{
    unsigned int offset;
    unsigned char const *yuv_source[3];
    int row;

    width &= ~1;
    height &= ~1;
    if(width <= 0 || height <= 0 ||
       destination_x < 0 || destination_y < 0 ||
       destination_x + width > LCD_WIDTH ||
       destination_y + height > LCD_HEIGHT)
        return false;

    offset = stride * source_y;
    yuv_source[0] = source[0] + offset + source_x;
    yuv_source[1] = source[1] + (offset >> 2) + (source_x >> 1);
    yuv_source[2] = source[2] + (yuv_source[1] - source[1]);

    for(row = 0; row < height; row += 2) {
        if(destination_x == 0 && width == LCD_WIDTH) {
            lcd_write_yuv420_lines(
                yuv_source,
                (uint16_t *)&crazypod_framebuffer[
                    destination_y + row][0],
                width, stride);
        }
        else {
            lcd_write_yuv420_lines(
                yuv_source, video_line_buffer, width, stride);
            memcpy(&crazypod_framebuffer[
                       destination_y + row][destination_x],
                   video_line_buffer, width * sizeof(uint16_t));
            memcpy(&crazypod_framebuffer[
                       destination_y + row + 1][destination_x],
                   video_line_buffer + width,
                   width * sizeof(uint16_t));
        }
        yuv_source[0] += stride << 1;
        yuv_source[1] += stride >> 1;
        yuv_source[2] += stride >> 1;
    }
    return true;
}
#endif

void lcd_puts(int x, int y, const unsigned char *string)
{
    (void)x;
    (void)y;
    (void)string;
}

void lcd_putsf(int x, int y, const unsigned char *format, ...)
{
    va_list arguments;
    (void)x;
    (void)y;
    va_start(arguments, format);
    va_end(arguments);
}

struct viewport *lcd_init_viewport(struct viewport *vp)
{
    if(vp == NULL)
        vp = &crazypod_viewport;

    vp->x = 0;
    vp->y = 0;
    vp->width = LCD_WIDTH;
    vp->height = LCD_HEIGHT;
    vp->buffer = &lcd_framebuffer_default;
    return vp;
}

struct viewport *lcd_set_viewport(struct viewport *vp)
{
    struct viewport *previous = lcd_current_viewport;

    lcd_current_viewport = vp == NULL ? &crazypod_viewport : vp;
    if(lcd_current_viewport->buffer == NULL)
        lcd_current_viewport->buffer = &lcd_framebuffer_default;
    return previous;
}

struct viewport *lcd_set_viewport_ex(struct viewport *vp, int flags)
{
    (void)flags;
    return lcd_set_viewport(vp);
}

void lcd_clear_display(void)
{
    memset(crazypod_framebuffer, 0, sizeof(crazypod_framebuffer));
}

void lcd_init(void)
{
    lcd_current_viewport = &crazypod_viewport;
    lcd_clear_display();
    lcd_init_device();
}

static void fill_screen(fb_data color)
{
    fb_data *pixel = &crazypod_framebuffer[0][0];
    fb_data *end = pixel + LCD_WIDTH * LCD_HEIGHT;

    while(pixel < end)
        *pixel++ = color;
}

void crazypod_lcd_show_boot_logo(void)
{
    fill_screen(LCD_BLACK);
    crazypod_boot_logo_draw(
        &crazypod_framebuffer[0][0],
        LCD_WIDTH, LCD_HEIGHT, LCD_WIDTH);
    lcd_update();
}

const lv_image_dsc_t *crazypod_lcd_boot_logo_image(void)
{
    if(boot_logo_descriptor.header.magic != LV_IMAGE_HEADER_MAGIC) {
        memset(&boot_logo_descriptor, 0, sizeof(boot_logo_descriptor));
        boot_logo_descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
        boot_logo_descriptor.header.cf = LV_COLOR_FORMAT_RGB565;
        boot_logo_descriptor.header.w = CRAZYPOD_BOOT_LOGO_WIDTH;
        boot_logo_descriptor.header.h = CRAZYPOD_BOOT_LOGO_HEIGHT;
        boot_logo_descriptor.header.stride =
            CRAZYPOD_BOOT_LOGO_WIDTH * sizeof(fb_data);
        boot_logo_descriptor.data_size =
            CRAZYPOD_BOOT_LOGO_WIDTH *
            CRAZYPOD_BOOT_LOGO_HEIGHT * sizeof(fb_data);
        boot_logo_descriptor.data =
            (const uint8_t *)crazypod_boot_logo_pixels();
    }
    return &boot_logo_descriptor;
}

static void draw_glyph(const lv_font_t *font, uint32_t codepoint,
                       int x, int line_y, fb_data color)
{
    lv_font_glyph_dsc_t glyph;
    const uint8_t *bitmap;
    int glyph_x;
    int glyph_y;
    int row;
    int column;

    if(!lv_font_get_glyph_dsc(font, &glyph, codepoint, 0))
        return;
    if(glyph.box_w == 0 || glyph.box_h == 0)
        return;

    glyph.req_raw_bitmap = 1;
    bitmap = glyph.resolved_font->get_glyph_bitmap(&glyph, NULL);
    if(bitmap == NULL || glyph.format != LV_FONT_GLYPH_FORMAT_A4)
        return;

    glyph_x = x + glyph.ofs_x;
    glyph_y = line_y + (font->line_height - font->base_line)
              - glyph.box_h - glyph.ofs_y;

    for(row = 0; row < glyph.box_h; ++row) {
        for(column = 0; column < glyph.box_w; ++column) {
            unsigned pixel_index = (unsigned)row * glyph.box_w + column;
            uint8_t packed = bitmap[pixel_index >> 1];
            uint8_t alpha = (pixel_index & 1) ? (packed & 0x0f)
                                             : (packed >> 4);
            int px = glyph_x + column;
            int py = glyph_y + row;

            if(alpha >= 5 && px >= 0 && px < LCD_WIDTH &&
               py >= 0 && py < LCD_HEIGHT)
                crazypod_framebuffer[py][px] = color;
        }
    }
}

static uint32_t next_utf8(const char **text)
{
    const unsigned char *cursor = (const unsigned char *)*text;
    uint32_t codepoint;

    if(cursor[0] < 0x80) {
        *text += 1;
        return cursor[0];
    }
    if((cursor[0] & 0xe0) == 0xc0 && (cursor[1] & 0xc0) == 0x80) {
        codepoint = ((uint32_t)(cursor[0] & 0x1f) << 6) |
            (uint32_t)(cursor[1] & 0x3f);
        *text += 2;
        return codepoint;
    }
    if((cursor[0] & 0xf0) == 0xe0 &&
       (cursor[1] & 0xc0) == 0x80 && (cursor[2] & 0xc0) == 0x80) {
        codepoint = ((uint32_t)(cursor[0] & 0x0f) << 12) |
            ((uint32_t)(cursor[1] & 0x3f) << 6) |
            (uint32_t)(cursor[2] & 0x3f);
        *text += 3;
        return codepoint;
    }
    if((cursor[0] & 0xf8) == 0xf0 &&
       (cursor[1] & 0xc0) == 0x80 && (cursor[2] & 0xc0) == 0x80 &&
       (cursor[3] & 0xc0) == 0x80) {
        codepoint = ((uint32_t)(cursor[0] & 0x07) << 18) |
            ((uint32_t)(cursor[1] & 0x3f) << 12) |
            ((uint32_t)(cursor[2] & 0x3f) << 6) |
            (uint32_t)(cursor[3] & 0x3f);
        *text += 4;
        return codepoint;
    }
    *text += 1;
    return 0xfffd;
}

static bool contains_non_ascii(const char *text)
{
    while(text != NULL && *text != '\0') {
        if((unsigned char)*text++ >= 0x80)
            return true;
    }
    return false;
}

static const lv_font_t *lcd_localized_font(const lv_font_t *font,
                                            const char *text)
{
    if(!contains_non_ascii(text))
        return font;
    if(font == &lv_font_montserrat_8)
        return &lv_font_crazypod_i18n_8;
    if(font == &lv_font_montserrat_10)
        return &lv_font_crazypod_i18n_10;
    if(font == &lv_font_montserrat_12)
        return &lv_font_crazypod_i18n_12;
    if(font == &lv_font_montserrat_16 || font == &lv_font_montserrat_24)
        return &lv_font_source_han_sans_sc_16_cjk;
    return font;
}

static void show_message(const char *title, const char *message,
                         fb_data background)
{
    const lv_font_t *title_font = &lv_font_montserrat_16;
    const lv_font_t *body_font = &lv_font_montserrat_12;
    const fb_data foreground = LCD_RGBPACK(255, 255, 255);
    const char *cursor;
    int x = 14;
    int y = 18;

    title = crazypod_l10n_text(title);
    message = crazypod_l10n_text(message);
    title_font = lcd_localized_font(title_font, title);
    body_font = lcd_localized_font(body_font, message);
    cursor = message;
    fill_screen(background);

    while(*title != '\0') {
        lv_font_glyph_dsc_t glyph;
        uint32_t codepoint = next_utf8(&title);

        if(lv_font_get_glyph_dsc(title_font, &glyph, codepoint, 0)) {
            draw_glyph(title_font, codepoint, x, y, foreground);
            x += glyph.adv_w;
        }
    }

    x = 14;
    y = 48;
    while(*cursor != '\0' && y + body_font->line_height < LCD_HEIGHT - 12) {
        const char *line_start = cursor;
        int line_width = x;

        while(*cursor != '\0' && *cursor != '\n') {
            lv_font_glyph_dsc_t glyph;
            const char *next = cursor;
            uint32_t codepoint = next_utf8(&next);

            if(!lv_font_get_glyph_dsc(body_font, &glyph, codepoint, 0)) {
                cursor = next;
                continue;
            }
            if(line_width + glyph.adv_w > LCD_WIDTH - 14)
                break;
            line_width += glyph.adv_w;
            cursor = next;
        }

        {
            const char *character = line_start;
            int draw_x = x;

            while(character < cursor) {
                lv_font_glyph_dsc_t glyph;
                uint32_t codepoint = next_utf8(&character);

                if(lv_font_get_glyph_dsc(body_font, &glyph, codepoint, 0)) {
                    draw_glyph(body_font, codepoint, draw_x, y, foreground);
                    draw_x += glyph.adv_w;
                }
            }
        }

        if(*cursor == '\n')
            ++cursor;
        y += body_font->line_height + 3;
    }

    lcd_update();
}

static int draw_text(const lv_font_t *font, const char *text,
                     int x, int y, int maximum_x, fb_data color)
{
    text = crazypod_l10n_text(text);
    font = lcd_localized_font(font, text);
    while(text != NULL && *text != '\0' && x < maximum_x) {
        lv_font_glyph_dsc_t glyph;
        uint32_t codepoint = next_utf8(&text);

        if(!lv_font_get_glyph_dsc(font, &glyph, codepoint, 0))
            continue;
        if(x + glyph.adv_w > maximum_x)
            break;
        draw_glyph(font, codepoint, x, y, color);
        x += glyph.adv_w;
    }
    return x;
}

void crazypod_lcd_draw_text(const char *text, int x, int y,
                            int maximum_x, uint32_t color)
{
    draw_text(&lv_font_montserrat_12, text, x, y, maximum_x,
              LCD_RGBPACK((color >> 16) & 255,
                          (color >> 8) & 255, color & 255));
}

static void format_video_time(char *buffer, size_t size, uint32_t seconds)
{
    uint32_t hours = seconds / 3600u;
    uint32_t minutes = seconds / 60u % 60u;
    uint32_t remainder = seconds % 60u;

    if(hours > 0)
        snprintf(buffer, size, "%lu:%02lu:%02lu",
                 (unsigned long)hours, (unsigned long)minutes,
                 (unsigned long)remainder);
    else
        snprintf(buffer, size, "%lu:%02lu",
                 (unsigned long)minutes, (unsigned long)remainder);
}

static void compose_video_controls(
    const char *title, uint32_t elapsed_seconds,
    uint32_t duration_seconds, int volume,
    bool paused, const char *message)
{
    const int panel_height = 42;
    const int panel_y = LCD_HEIGHT - panel_height;
    const fb_data white = LCD_RGBPACK(255, 255, 255);
    const fb_data muted = LCD_RGBPACK(154, 160, 168);
    const fb_data accent = LCD_RGBPACK(52, 120, 246);
    char elapsed[20];
    char duration[20];
    char volume_text[20];
    int progress_width = 0;
    int row;

    for(row = panel_y; row < LCD_HEIGHT; ++row)
        memset(crazypod_framebuffer[row], 0,
               sizeof(crazypod_framebuffer[row]));
    for(row = 0; row < LCD_WIDTH; ++row)
        crazypod_framebuffer[panel_y][row] =
            LCD_RGBPACK(42, 46, 53);

    draw_text(&lv_font_montserrat_12,
              paused ? CP_TR("PAUSED") : CP_TR("PLAYING"),
              8, panel_y + 3, 62, paused ? muted : accent);
    if(message != NULL && message[0] != '\0')
        draw_text(&lv_font_montserrat_12, message,
                  66, panel_y + 3, 312, white);
    else
        draw_text(&lv_font_montserrat_12, title,
                  66, panel_y + 3, 312, white);

    for(row = 8; row < 312; ++row) {
        crazypod_framebuffer[panel_y + 19][row] =
            LCD_RGBPACK(48, 52, 60);
        crazypod_framebuffer[panel_y + 20][row] =
            LCD_RGBPACK(48, 52, 60);
        crazypod_framebuffer[panel_y + 21][row] =
            LCD_RGBPACK(48, 52, 60);
    }
    if(duration_seconds > 0) {
        progress_width =
            (int)((uint64_t)elapsed_seconds * 304u / duration_seconds);
        if(progress_width > 304)
            progress_width = 304;
    }
    for(row = 8; row < 8 + progress_width; ++row) {
        crazypod_framebuffer[panel_y + 19][row] = accent;
        crazypod_framebuffer[panel_y + 20][row] = accent;
        crazypod_framebuffer[panel_y + 21][row] = accent;
    }

    format_video_time(elapsed, sizeof(elapsed), elapsed_seconds);
    format_video_time(duration, sizeof(duration), duration_seconds);
    snprintf(volume_text, sizeof(volume_text), CP_FMT("VOL %d"), volume);
    draw_text(&lv_font_montserrat_12, elapsed,
              8, panel_y + 25, 70, muted);
    draw_text(&lv_font_montserrat_12, duration,
              72, panel_y + 25, 134, muted);
    draw_text(&lv_font_montserrat_12,
              CP_TR("PLAY  -10s  +10s  MENU"),
              136, panel_y + 25, 266, muted);
    draw_text(&lv_font_montserrat_12, volume_text,
              268, panel_y + 25, 318, white);
}

void crazypod_lcd_draw_video_frame(
    unsigned char * const source[3],
    int source_x, int source_y, int stride,
    int destination_x, int destination_y,
    int width, int height,
    bool controls_visible, const char *title,
    uint32_t elapsed_seconds, uint32_t duration_seconds,
    int volume, bool paused, const char *message)
{
    memset(crazypod_framebuffer, 0, sizeof(crazypod_framebuffer));
    if(!blit_yuv_to_framebuffer(
           source, source_x, source_y, stride,
           destination_x, destination_y, width, height))
        return;
    if(controls_visible)
        compose_video_controls(
            title, elapsed_seconds, duration_seconds,
            volume, paused, message);
    lcd_update();
}

void crazypod_lcd_show_panic(const char *message)
{
    show_message(CP_TR("CRAZYPOD PANIC"), message,
                 LCD_RGBPACK(132, 20, 35));
}

#endif
