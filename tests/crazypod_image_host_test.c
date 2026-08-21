#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "crazypod_image.h"

#define ARRAYLEN(array) (sizeof(array) / sizeof((array)[0]))
#define TEST_SOURCE_WIDTH 19
#define TEST_SOURCE_HEIGHT 15
#define TEST_MAX_PIXELS 512

static int clamp_coordinate(int value, int maximum)
{
    if(value < 0)
        return 0;
    if(value >= maximum)
        return maximum - 1;
    return value;
}

static void reference_glass(
    const fb_data *source, int source_width, int source_height,
    int source_stride, int source_x, int source_y,
    int source_region_width, int source_region_height,
    uint32_t tint, unsigned tint_opa,
    fb_data *sample, fb_data *scratch, fb_data *destination,
    int destination_width, int destination_height)
{
    int sample_width =
        (destination_width + CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE - 1) /
        CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE;
    int sample_height =
        (destination_height + CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE - 1) /
        CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE;
    unsigned tint_red = (tint >> 16) & 0xff;
    unsigned tint_green = (tint >> 8) & 0xff;
    unsigned tint_blue = tint & 0xff;
    int y;

    for(y = 0; y < sample_height; ++y) {
        int destination_y0 = y * CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE;
        int destination_y1 = destination_y0 +
            CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE;
        int sy0;
        int sy1;
        int x;

        if(destination_y1 > destination_height)
            destination_y1 = destination_height;
        sy0 = source_y + destination_y0 * source_region_height /
            destination_height;
        sy1 = source_y + destination_y1 * source_region_height /
            destination_height;
        if(sy1 <= sy0)
            sy1 = sy0 + 1;
        for(x = 0; x < sample_width; ++x) {
            int destination_x0 = x * CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE;
            int destination_x1 = destination_x0 +
                CRAZYPOD_IMAGE_GLASS_SAMPLE_SCALE;
            int sx0;
            int sx1;
            unsigned red = 0;
            unsigned green = 0;
            unsigned blue = 0;
            unsigned samples = 0;
            int sy;

            if(destination_x1 > destination_width)
                destination_x1 = destination_width;
            sx0 = source_x + destination_x0 * source_region_width /
                destination_width;
            sx1 = source_x + destination_x1 * source_region_width /
                destination_width;
            if(sx1 <= sx0)
                sx1 = sx0 + 1;
            for(sy = sy0; sy < sy1; ++sy) {
                int source_sample_y =
                    clamp_coordinate(sy, source_height);
                int sx;

                for(sx = sx0; sx < sx1; ++sx) {
                    int source_sample_x =
                        clamp_coordinate(sx, source_width);
                    fb_data pixel = source[
                        source_sample_y * source_stride + source_sample_x];

                    red += RGB_UNPACK_RED(pixel);
                    green += RGB_UNPACK_GREEN(pixel);
                    blue += RGB_UNPACK_BLUE(pixel);
                    ++samples;
                }
            }
            sample[y * sample_width + x] = LCD_RGBPACK(
                red / samples, green / samples, blue / samples);
        }
    }

    for(y = 0; y < sample_height; ++y) {
        int x;

        for(x = 0; x < sample_width; ++x) {
            unsigned red = 0;
            unsigned green = 0;
            unsigned blue = 0;
            int offset;

            for(offset = -2; offset <= 2; ++offset) {
                fb_data pixel = sample[
                    y * sample_width +
                    clamp_coordinate(x + offset, sample_width)];

                red += RGB_UNPACK_RED(pixel);
                green += RGB_UNPACK_GREEN(pixel);
                blue += RGB_UNPACK_BLUE(pixel);
            }
            scratch[y * sample_width + x] = LCD_RGBPACK(
                red / 5, green / 5, blue / 5);
        }
    }

    for(y = 0; y < sample_height; ++y) {
        int x;

        for(x = 0; x < sample_width; ++x) {
            unsigned red = 0;
            unsigned green = 0;
            unsigned blue = 0;
            int offset;

            for(offset = -2; offset <= 2; ++offset) {
                fb_data pixel = scratch[
                    clamp_coordinate(y + offset, sample_height) *
                        sample_width + x];

                red += RGB_UNPACK_RED(pixel);
                green += RGB_UNPACK_GREEN(pixel);
                blue += RGB_UNPACK_BLUE(pixel);
            }
            red /= 5;
            green /= 5;
            blue /= 5;
            sample[y * sample_width + x] = LCD_RGBPACK(
                (red * (255 - tint_opa) +
                 tint_red * tint_opa + 127) / 255,
                (green * (255 - tint_opa) +
                 tint_green * tint_opa + 127) / 255,
                (blue * (255 - tint_opa) +
                 tint_blue * tint_opa + 127) / 255);
        }
    }
    assert(crazypod_image_scale_rgb565(
        sample, sample_width, sample_height, sample_width,
        destination, destination_width, destination_height));
}

static void fill_source(fb_data *source)
{
    int y;

    for(y = 0; y < TEST_SOURCE_HEIGHT; ++y) {
        int x;

        for(x = 0; x < TEST_SOURCE_WIDTH; ++x) {
            source[y * TEST_SOURCE_WIDTH + x] = LCD_RGBPACK(
                (unsigned)(x * 37 + y * 11) & 255,
                (unsigned)(x * 17 + y * 29) & 255,
                (unsigned)(x * 7 + y * 43) & 255);
        }
    }
}

static void assert_close(
    const fb_data *actual, const fb_data *expected, size_t count)
{
    int max_red = 0;
    int max_green = 0;
    int max_blue = 0;
    size_t index;

    for(index = 0; index < count; ++index) {
        int red = abs((int)RGB_UNPACK_RED(actual[index]) -
                      (int)RGB_UNPACK_RED(expected[index]));
        int green = abs((int)RGB_UNPACK_GREEN(actual[index]) -
                        (int)RGB_UNPACK_GREEN(expected[index]));
        int blue = abs((int)RGB_UNPACK_BLUE(actual[index]) -
                       (int)RGB_UNPACK_BLUE(expected[index]));

        if(red > max_red)
            max_red = red;
        if(green > max_green)
            max_green = green;
        if(blue > max_blue)
            max_blue = blue;
    }
    assert(max_red <= 9);
    assert(max_green <= 4);
    assert(max_blue <= 9);
}

static void test_case(
    const fb_data *source, int source_x, int source_y,
    int source_region_width, int source_region_height,
    int destination_width, int destination_height)
{
    fb_data actual_samples[TEST_MAX_PIXELS];
    fb_data actual_scratch[TEST_MAX_PIXELS];
    fb_data expected_samples[TEST_MAX_PIXELS];
    fb_data expected_scratch[TEST_MAX_PIXELS];
    fb_data actual[TEST_MAX_PIXELS];
    fb_data expected[TEST_MAX_PIXELS];
    size_t sample_count = crazypod_image_glass_sample_pixels(
        destination_width, destination_height);
    size_t output_count =
        (size_t)destination_width * destination_height;

    assert(sample_count <= ARRAYLEN(actual_samples));
    assert(output_count <= ARRAYLEN(actual));
    assert(crazypod_image_render_glass_rgb565(
        source, TEST_SOURCE_WIDTH, TEST_SOURCE_HEIGHT,
        TEST_SOURCE_WIDTH, source_x, source_y,
        source_region_width, source_region_height,
        0x11131a, 104,
        actual_samples, actual_scratch, ARRAYLEN(actual_samples),
        actual, destination_width, destination_height));
    reference_glass(
        source, TEST_SOURCE_WIDTH, TEST_SOURCE_HEIGHT,
        TEST_SOURCE_WIDTH, source_x, source_y,
        source_region_width, source_region_height,
        0x11131a, 104,
        expected_samples, expected_scratch, expected,
        destination_width, destination_height);
    assert(memcmp(actual_samples, expected_samples,
                  sample_count * sizeof(*actual_samples)) == 0);
    assert_close(actual, expected, output_count);
}

static void test_constant(void)
{
    fb_data source[TEST_SOURCE_WIDTH * TEST_SOURCE_HEIGHT];
    fb_data samples[16];
    fb_data scratch[16];
    fb_data destination[63];
    size_t index;

    for(index = 0; index < ARRAYLEN(source); ++index)
        source[index] = LCD_RGBPACK(96, 144, 208);
    assert(crazypod_image_render_glass_rgb565(
        source, TEST_SOURCE_WIDTH, TEST_SOURCE_HEIGHT,
        TEST_SOURCE_WIDTH, 3, 2, 9, 7,
        0x11131a, 104,
        samples, scratch, ARRAYLEN(samples),
        destination, 9, 7));
    for(index = 1; index < ARRAYLEN(destination); ++index)
        assert(destination[index] == destination[0]);
}

static void test_solid_source(void)
{
    fb_data source = LCD_RGBPACK(96, 144, 208);
    fb_data samples[12];
    fb_data scratch[12];
    fb_data destination[143];
    size_t index;

    assert(crazypod_image_render_glass_rgb565(
        &source, 1, 1, 1, 0, 0, 13, 11,
        0x11131a, 104,
        samples, scratch, ARRAYLEN(samples),
        destination, 13, 11));
    for(index = 1; index < ARRAYLEN(destination); ++index)
        assert(destination[index] == destination[0]);
}

int main(void)
{
    fb_data source[TEST_SOURCE_WIDTH * TEST_SOURCE_HEIGHT];

    fill_source(source);
    test_case(source, 2, 1, 13, 11, 13, 11);
    test_case(source, 1, 2, 15, 10, 17, 13);
    test_case(source, 4, 3, 9, 7, 9, 7);
    test_constant();
    test_solid_source();
    puts("crazypod image host tests passed");
    return 0;
}
