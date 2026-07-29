#include "config.h"

#ifdef IPOD_6G

#include <stdint.h>
#include <string.h>

#include "lcd.h"

#include "crazypod_artwork_palette.h"

#define ARTWORK_PALETTE_BUCKETS 24
#define ARTWORK_PALETTE_SAMPLE_SIDE 36
#define ARTWORK_HUE_RANGE (ARTWORK_PALETTE_BUCKETS * 64)

struct color_bucket {
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    uint32_t weight;
};

struct color_sample {
    unsigned red;
    unsigned green;
    unsigned blue;
    unsigned hue;
    unsigned saturation;
    unsigned brightness;
    uint32_t score;
    bool valid;
};

static unsigned maximum3(unsigned first, unsigned second, unsigned third)
{
    unsigned result = first > second ? first : second;

    return result > third ? result : third;
}

static unsigned minimum3(unsigned first, unsigned second, unsigned third)
{
    unsigned result = first < second ? first : second;

    return result < third ? result : third;
}

static void rgb_to_hsv(
    unsigned red, unsigned green, unsigned blue,
    unsigned *hue, unsigned *saturation, unsigned *brightness)
{
    unsigned maximum = maximum3(red, green, blue);
    unsigned minimum = minimum3(red, green, blue);
    unsigned delta = maximum - minimum;
    int value = 0;

    *brightness = maximum;
    *saturation = maximum > 0 ? delta * 255 / maximum : 0;
    if(delta == 0) {
        *hue = 0;
        return;
    }
    if(maximum == red)
        value = 256 * ((int)green - (int)blue) / (int)delta;
    else if(maximum == green)
        value = 512 +
            256 * ((int)blue - (int)red) / (int)delta;
    else
        value = 1024 +
            256 * ((int)red - (int)green) / (int)delta;
    while(value < 0)
        value += ARTWORK_HUE_RANGE;
    *hue = (unsigned)value % ARTWORK_HUE_RANGE;
}

static unsigned hue_distance(unsigned first, unsigned second)
{
    unsigned direct = first > second ? first - second : second - first;
    unsigned wrapped = ARTWORK_HUE_RANGE - direct;

    return direct < wrapped ? direct : wrapped;
}

static unsigned clamp_channel(int value)
{
    if(value < 0)
        return 0;
    if(value > 255)
        return 255;
    return (unsigned)value;
}

static uint32_t wave_color(
    const struct color_sample *sample,
    unsigned saturation_percent,
    unsigned brightness_percent,
    unsigned minimum_brightness)
{
    unsigned maximum =
        maximum3(sample->red, sample->green, sample->blue);
    unsigned target = maximum * brightness_percent / 100;
    int red;
    int green;
    int blue;

    if(target < minimum_brightness)
        target = minimum_brightness;
    if(target > 255)
        target = 255;
    if(maximum == 0)
        return 0xFFFFFF;
    red = (int)(sample->red * target / maximum);
    green = (int)(sample->green * target / maximum);
    blue = (int)(sample->blue * target / maximum);
    red = (int)target -
        ((int)target - red) * (int)saturation_percent / 100;
    green = (int)target -
        ((int)target - green) * (int)saturation_percent / 100;
    blue = (int)target -
        ((int)target - blue) * (int)saturation_percent / 100;
    return (clamp_channel(red) << 16) |
           (clamp_channel(green) << 8) |
           clamp_channel(blue);
}

void crazypod_artwork_palette_fallback(
    struct crazypod_artwork_palette *palette,
    uint32_t primary, uint32_t secondary)
{
    if(palette == NULL)
        return;
    palette->primary = primary;
    palette->secondary = secondary;
    palette->highlight = 0xFFFFFF;
}

bool crazypod_artwork_palette_extract(
    const lv_image_dsc_t *artwork,
    struct crazypod_artwork_palette *palette)
{
    struct color_bucket buckets[ARTWORK_PALETTE_BUCKETS];
    struct color_sample samples[ARTWORK_PALETTE_BUCKETS];
    const fb_data *pixels;
    int width;
    int height;
    int stride;
    int step_x;
    int step_y;
    int primary = -1;
    int secondary = -1;
    int highlight = -1;
    int y;
    int index;

    if(artwork == NULL || palette == NULL ||
       artwork->data == NULL ||
       artwork->header.cf != LV_COLOR_FORMAT_RGB565 ||
       artwork->header.w == 0 || artwork->header.h == 0 ||
       artwork->header.stride <
           artwork->header.w * sizeof(fb_data))
        return false;

    memset(buckets, 0, sizeof(buckets));
    memset(samples, 0, sizeof(samples));
    pixels = (const fb_data *)artwork->data;
    width = artwork->header.w;
    height = artwork->header.h;
    stride = artwork->header.stride / sizeof(fb_data);
    step_x = (width + ARTWORK_PALETTE_SAMPLE_SIDE - 1) /
        ARTWORK_PALETTE_SAMPLE_SIDE;
    step_y = (height + ARTWORK_PALETTE_SAMPLE_SIDE - 1) /
        ARTWORK_PALETTE_SAMPLE_SIDE;

    for(y = 0; y < height; y += step_y) {
        int x;

        for(x = 0; x < width; x += step_x) {
            fb_data pixel = pixels[y * stride + x];
            unsigned red = FB_UNPACK_RED(pixel);
            unsigned green = FB_UNPACK_GREEN(pixel);
            unsigned blue = FB_UNPACK_BLUE(pixel);
            unsigned hue;
            unsigned saturation;
            unsigned brightness;
            unsigned weight;
            struct color_bucket *bucket;

            rgb_to_hsv(
                red, green, blue,
                &hue, &saturation, &brightness);
            if(brightness <= 20 || saturation <= 15)
                continue;
            weight =
                (saturation > 31 ? saturation : 31) *
                (72 + brightness * 183 / 255) / 255;
            bucket = &buckets[
                hue * ARTWORK_PALETTE_BUCKETS /
                ARTWORK_HUE_RANGE];
            bucket->red += red * weight;
            bucket->green += green * weight;
            bucket->blue += blue * weight;
            bucket->weight += weight;
        }
    }

    for(index = 0; index < ARTWORK_PALETTE_BUCKETS; ++index) {
        struct color_bucket *bucket = &buckets[index];
        struct color_sample *sample = &samples[index];

        if(bucket->weight == 0)
            continue;
        sample->red = bucket->red / bucket->weight;
        sample->green = bucket->green / bucket->weight;
        sample->blue = bucket->blue / bucket->weight;
        rgb_to_hsv(
            sample->red, sample->green, sample->blue,
            &sample->hue, &sample->saturation,
            &sample->brightness);
        sample->score = bucket->weight;
        sample->valid = true;
        if(primary < 0 ||
           sample->score > samples[primary].score)
            primary = index;
    }
    if(primary < 0)
        return false;

    for(index = 0; index < ARTWORK_PALETTE_BUCKETS; ++index) {
        const struct color_sample *sample = &samples[index];

        if(!sample->valid || index == primary)
            continue;
        if(secondary < 0 ||
           (hue_distance(sample->hue, samples[primary].hue) >= 184 &&
            (hue_distance(
                 samples[secondary].hue,
                 samples[primary].hue) < 184 ||
             sample->score > samples[secondary].score)))
            secondary = index;
        if(sample->brightness >= 115 &&
           hue_distance(sample->hue, samples[primary].hue) >= 77 &&
           (highlight < 0 ||
            sample->score > samples[highlight].score))
            highlight = index;
    }
    if(secondary < 0)
        secondary = primary;
    if(highlight < 0) {
        highlight = primary;
        for(index = 0; index < ARTWORK_PALETTE_BUCKETS; ++index) {
            if(samples[index].valid &&
               samples[index].brightness >
                   samples[highlight].brightness)
                highlight = index;
        }
    }

    palette->primary = wave_color(
        &samples[primary], 118, 108, 148);
    palette->secondary = wave_color(
        &samples[secondary], 122, 104, 138);
    palette->highlight = wave_color(
        &samples[highlight], 92, 122, 173);
    return true;
}

#endif
