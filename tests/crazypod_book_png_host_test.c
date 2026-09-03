#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "crazypod_book_png.h"
#include "lcd.h"

void __assert(const char *file, int line, const char *expression)
{
    fprintf(stderr, "%s:%d: assertion failed: %s\n",
            file, line, expression);
    abort();
}

int main(int argc, char **argv)
{
    struct crazypod_book_png_info info;
    fb_data pixels[16];
    void *workspace;
    int width;
    int height;

    if(argc != 3) {
        fprintf(stderr, "usage: %s PNG BROKEN_PNG\n", argv[0]);
        return 2;
    }
    assert(crazypod_book_png_inspect(argv[1], &info));
    assert(info.width == 3 && info.height == 2);
    assert(info.workspace_size > 0 && info.workspace_size <= 96 * 1024);
    workspace = malloc(info.workspace_size);
    assert(workspace != NULL);
    assert(crazypod_book_png_decode(
        argv[1], 3, 2, pixels, &width, &height,
        workspace, info.workspace_size));
    assert(width == 3 && height == 2);
    assert(RGB_UNPACK_RED(pixels[0]) > 200);
    assert(RGB_UNPACK_GREEN(pixels[1]) > 200);
    assert(RGB_UNPACK_BLUE(pixels[2]) > 200);
    assert(RGB_UNPACK_RED(pixels[3]) > 200);
    assert(RGB_UNPACK_GREEN(pixels[4]) > 200);
    assert(RGB_UNPACK_RED(pixels[5]) > 100 &&
           RGB_UNPACK_RED(pixels[5]) < 200);
    assert(crazypod_book_png_decode(
        argv[1], 2, 2, pixels, &width, &height,
        workspace, info.workspace_size));
    assert(width == 2 && height == 1);
    free(workspace);
    assert(!crazypod_book_png_inspect(argv[2], &info));
    puts("PNG streaming decode, filtering, alpha, and fit tests passed");
    return 0;
}
