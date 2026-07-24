#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <stdarg.h>
#include <string.h>

#include "lcd.h"
#include "system.h"

#include "lvgl.h"

#include "crazypod_lcd.h"

static fb_data crazypod_framebuffer[LCD_FBHEIGHT][LCD_FBWIDTH]
    IRAM_LCDFRAMEBUFFER CACHEALIGN_AT_LEAST_ATTR(16);

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

void crazypod_lcd_show_panic(const char *message)
{
    show_message("CRAZYPOD PANIC", message,
                 LCD_RGBPACK(132, 20, 35));
}

#endif
