#ifndef CRAZYPOD_UI_MENU_LAYOUT_H
#define CRAZYPOD_UI_MENU_LAYOUT_H

int crazypod_ui_menu_window_start(
    int count, int selected, int visible_rows);
void crazypod_ui_menu_scroll_thumb(
    int count, int selected, int visible_rows,
    int track_y, int track_height, int minimum_height,
    int *thumb_y, int *thumb_height);

#endif
