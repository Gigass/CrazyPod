#include <stdbool.h>
#include <string.h>

#include "crazypod_ui_text.h"

#define CRAZYPOD_NOTE_WRAP_COLUMNS 34
#define CRAZYPOD_NOTE_WINDOW_LINES 9

int crazypod_ui_text_character_size(const char *text)
{
    unsigned char value = (unsigned char)text[0];

    if((value & 0x80) == 0)
        return 1;
    if((value & 0xe0) == 0xc0 && text[1] != '\0')
        return 2;
    if((value & 0xf0) == 0xe0 && text[1] != '\0' &&
       text[2] != '\0')
        return 3;
    if((value & 0xf8) == 0xf0 && text[1] != '\0' &&
       text[2] != '\0' && text[3] != '\0')
        return 4;
    return 1;
}

int crazypod_ui_text_note_line_count(const char *body)
{
    const char *cursor = body;
    int column = 0;
    int lines = 1;

    while(cursor != NULL && *cursor != '\0') {
        int bytes;
        if(*cursor == '\n') {
            ++lines;
            column = 0;
            ++cursor;
            continue;
        }
        bytes = crazypod_ui_text_character_size(cursor);
        cursor += bytes;
        ++column;
        if(column >= CRAZYPOD_NOTE_WRAP_COLUMNS) {
            ++lines;
            column = 0;
        }
    }
    return lines;
}

void crazypod_ui_text_note_window(const char *body, int first_line,
                                  char *output, size_t size)
{
    const char *cursor = body;
    size_t used = 0;
    int line = 0;
    int column = 0;

    if(size == 0)
        return;
    while(cursor != NULL && *cursor != '\0' &&
          line < first_line + CRAZYPOD_NOTE_WINDOW_LINES) {
        int bytes;
        bool newline = *cursor == '\n';

        if(newline)
            bytes = 1;
        else
            bytes = crazypod_ui_text_character_size(cursor);
        if(line >= first_line) {
            int i;
            for(i = 0; i < bytes && used + 1 < size; ++i)
                output[used++] = cursor[i];
        }
        cursor += bytes;
        if(newline || ++column >= CRAZYPOD_NOTE_WRAP_COLUMNS) {
            if(!newline && line >= first_line && used + 1 < size)
                output[used++] = '\n';
            ++line;
            column = 0;
        }
    }
    output[used] = '\0';
}

const char *crazypod_ui_text_with_cursor(const char *text, size_t cursor,
                                         char *output, size_t size)
{
    size_t length;

    if(output == NULL || size < 2)
        return text;
    length = strlen(text);
    if(cursor > length)
        cursor = length;
    if(length + 2 > size)
        length = size - 2;
    if(cursor > length)
        cursor = length;
    memcpy(output, text, cursor);
    output[cursor] = '|';
    memcpy(output + cursor + 1, text + cursor, length - cursor);
    output[length + 1] = '\0';
    return output;
}

void crazypod_ui_text_append(char *buffer, size_t size, const char *text)
{
    size_t used = strlen(buffer);
    size_t available;
    size_t copied = 0;

    if(used >= size - 1 || text == NULL)
        return;
    available = size - used - 1;
    while(copied < available && text[copied] != '\0') {
        buffer[used + copied] = text[copied];
        ++copied;
    }
    buffer[used + copied] = '\0';
}

void crazypod_ui_text_backspace(char *buffer)
{
    size_t length = strlen(buffer);

    if(length == 0)
        return;
    --length;
    while(length > 0 &&
          ((unsigned char)buffer[length] & 0xc0) == 0x80)
        --length;
    buffer[length] = '\0';
}

void crazypod_ui_text_insert(char *buffer, size_t size,
                             size_t *cursor, const char *text)
{
    size_t length = strlen(buffer);
    size_t insert_length = text != NULL ? strlen(text) : 0;

    if(cursor == NULL || insert_length == 0 || length >= size - 1)
        return;
    if(*cursor > length)
        *cursor = length;
    if(insert_length > size - length - 1)
        insert_length = size - length - 1;
    memmove(buffer + *cursor + insert_length,
            buffer + *cursor, length - *cursor + 1);
    memcpy(buffer + *cursor, text, insert_length);
    *cursor += insert_length;
}

void crazypod_ui_text_backspace_at(char *buffer, size_t *cursor)
{
    size_t start;
    size_t length;

    if(cursor == NULL || *cursor == 0)
        return;
    length = strlen(buffer);
    if(*cursor > length)
        *cursor = length;
    start = *cursor - 1;
    while(start > 0 &&
          ((unsigned char)buffer[start] & 0xc0) == 0x80)
        --start;
    memmove(buffer + start, buffer + *cursor,
            length - *cursor + 1);
    *cursor = start;
}

void crazypod_ui_text_move_cursor(const char *buffer, size_t *cursor,
                                  int direction)
{
    size_t length;

    if(buffer == NULL || cursor == NULL || direction == 0)
        return;
    length = strlen(buffer);
    if(*cursor > length)
        *cursor = length;
    if(direction < 0) {
        if(*cursor == 0)
            return;
        --*cursor;
        while(*cursor > 0 &&
              ((unsigned char)buffer[*cursor] & 0xc0) == 0x80)
            --*cursor;
    }
    else {
        if(*cursor >= length)
            return;
        ++*cursor;
        while(*cursor < length &&
              ((unsigned char)buffer[*cursor] & 0xc0) == 0x80)
            ++*cursor;
    }
}
