#ifndef CRAZYPOD_UI_TEXT_H
#define CRAZYPOD_UI_TEXT_H

#include <stddef.h>

int crazypod_ui_text_character_size(const char *text);
int crazypod_ui_text_note_line_count(const char *body);
void crazypod_ui_text_note_window(const char *body, int first_line,
                                  char *output, size_t size);
const char *crazypod_ui_text_with_cursor(const char *text, size_t cursor,
                                         char *output, size_t size);

void crazypod_ui_text_append(char *buffer, size_t size, const char *text);
void crazypod_ui_text_backspace(char *buffer);
void crazypod_ui_text_insert(char *buffer, size_t size,
                             size_t *cursor, const char *text);
void crazypod_ui_text_backspace_at(char *buffer, size_t *cursor);
void crazypod_ui_text_move_cursor(const char *buffer, size_t *cursor,
                                  int direction);

#endif
