#include "crazypod_ui_menu_layout.h"

int crazypod_ui_menu_window_start(
    int count, int selected, int visible_rows)
{
    int start;

    if(count <= visible_rows)
        return 0;
    start = selected - visible_rows / 2;
    if(start < 0)
        return 0;
    if(start > count - visible_rows)
        return count - visible_rows;
    return start;
}

void crazypod_ui_menu_scroll_thumb(
    int count, int selected, int visible_rows,
    int track_y, int track_height, int minimum_height,
    int *thumb_y, int *thumb_height)
{
    int height;
    int y;

    if(count <= 1 || visible_rows <= 0 || track_height <= 0) {
        if(thumb_y != 0)
            *thumb_y = track_y;
        if(thumb_height != 0)
            *thumb_height = track_height;
        return;
    }

    height = track_height * visible_rows / count;
    if(height < minimum_height)
        height = minimum_height;
    if(height > track_height)
        height = track_height;
    if(selected < 0)
        selected = 0;
    if(selected >= count)
        selected = count - 1;
    y = track_y + (track_height - height) * selected / (count - 1);

    if(thumb_y != 0)
        *thumb_y = y;
    if(thumb_height != 0)
        *thumb_height = height;
}
