#ifndef CRAZYPOD_BOOK_READER_INPUT_H
#define CRAZYPOD_BOOK_READER_INPUT_H

#include "../../navigation/crazypod_input_event.h"

struct crazypod_book_reader_input_actions {
    bool (*turn_page)(int direction);
    void (*refresh)(void);
    void (*show_actions)(void);
    void (*toggle_bookmark)(void);
    void (*leave)(void);
};

void crazypod_book_reader_input_handle(
    const struct crazypod_input_event *event,
    const struct crazypod_book_reader_input_actions *actions);

#endif
