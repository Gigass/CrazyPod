#ifndef CRAZYPOD_UI_TEXT_H
#define CRAZYPOD_UI_TEXT_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Writes a wall-clock time, either "14:05" or "2:05 PM", according to the
 * clock setting the caller passes in. Kept here, away from the settings
 * header, so the six places that draw a clock agree without each one
 * growing its own copy of the twelve-hour arithmetic -- midnight and noon
 * are where hand-rolled versions go wrong.
 *
 * The meridiem is the plain English AM/PM the original firmware used; it
 * is not translated.
 */
const char *crazypod_ui_text_clock(char *output, size_t size,
                                   int hour, int minute, int second,
                                   bool with_seconds, bool twelve_hour);

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
