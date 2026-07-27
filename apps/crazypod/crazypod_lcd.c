#include "config.h"

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

void lcd_blit_yuv(unsigned char * const source[3],
                  int source_x, int source_y, int stride,
                  int destination_x, int destination_y,
                  int width, int height)
{
    int row;
    int column;

    if(destination_x < 0 || destination_y < 0 ||
       destination_x + width > LCD_WIDTH ||
       destination_y + height > LCD_HEIGHT)
        return;

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
    lcd_update_rect(destination_x, destination_y, width, height);
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

static void show_message(const char *title, const char *message,
                         fb_data background)
{
    const lv_font_t *title_font = &lv_font_montserrat_12;
    const lv_font_t *body_font = &lv_font_montserrat_8;
    const fb_data foreground = LCD_RGBPACK(255, 255, 255);
    const char *cursor = message;
    int x = 14;
    int y = 18;

    fill_screen(background);

    while(*title != '\0') {
        lv_font_glyph_dsc_t glyph;
        unsigned char codepoint = (unsigned char)*title++;

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
            unsigned char codepoint = (unsigned char)*cursor;

            if(!lv_font_get_glyph_dsc(body_font, &glyph, codepoint, 0)) {
                ++cursor;
                continue;
            }
            if(line_width + glyph.adv_w > LCD_WIDTH - 14)
                break;
            line_width += glyph.adv_w;
            ++cursor;
        }

        {
            const char *character = line_start;
            int draw_x = x;

            while(character < cursor) {
                lv_font_glyph_dsc_t glyph;
                unsigned char codepoint = (unsigned char)*character++;

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
    while(text != NULL && *text != '\0' && x < maximum_x) {
        lv_font_glyph_dsc_t glyph;
        unsigned char codepoint = (unsigned char)*text++;

        if(!lv_font_get_glyph_dsc(font, &glyph, codepoint, 0))
            continue;
        if(x + glyph.adv_w > maximum_x)
            break;
        draw_glyph(font, codepoint, x, y, color);
        x += glyph.adv_w;
    }
    return x;
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

void crazypod_lcd_draw_video_controls(
    const char *title, uint32_t elapsed_seconds,
    uint32_t duration_seconds, int volume,
    bool paused, const char *message)
{
    const int panel_y = 198;
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

    draw_text(&lv_font_montserrat_8,
              paused ? "PAUSED" : "PLAYING",
              8, 201, 62, paused ? muted : accent);
    if(message != NULL && message[0] != '\0')
        draw_text(&lv_font_montserrat_8, message,
                  66, 201, 312, white);
    else
        draw_text(&lv_font_montserrat_8, title,
                  66, 201, 312, white);

    for(row = 8; row < 312; ++row) {
        crazypod_framebuffer[218][row] =
            LCD_RGBPACK(48, 52, 60);
        crazypod_framebuffer[219][row] =
            LCD_RGBPACK(48, 52, 60);
        crazypod_framebuffer[220][row] =
            LCD_RGBPACK(48, 52, 60);
    }
    if(duration_seconds > 0) {
        progress_width =
            (int)((uint64_t)elapsed_seconds * 304u / duration_seconds);
        if(progress_width > 304)
            progress_width = 304;
    }
    for(row = 8; row < 8 + progress_width; ++row) {
        crazypod_framebuffer[218][row] = accent;
        crazypod_framebuffer[219][row] = accent;
        crazypod_framebuffer[220][row] = accent;
    }

    format_video_time(elapsed, sizeof(elapsed), elapsed_seconds);
    format_video_time(duration, sizeof(duration), duration_seconds);
    snprintf(volume_text, sizeof(volume_text), "VOL %d", volume);
    draw_text(&lv_font_montserrat_8, elapsed, 8, 225, 75, muted);
    draw_text(&lv_font_montserrat_8, duration, 78, 225, 152, muted);
    draw_text(&lv_font_montserrat_8,
              "PLAY  -10s  +10s  MENU",
              158, 225, 276, muted);
    draw_text(&lv_font_montserrat_8, volume_text,
              278, 225, 318, white);
    lcd_update_rect(0, panel_y, LCD_WIDTH, LCD_HEIGHT - panel_y);
}

void crazypod_lcd_show_panic(const char *message)
{
    show_message("CRAZYPOD PANIC", message,
                 LCD_RGBPACK(132, 20, 35));
}

#endif
