#ifndef CRAZYPOD_BOOK_SESSION_H
#define CRAZYPOD_BOOK_SESSION_H

#include <stdbool.h>
#include <stdint.h>

void crazypod_book_session_reset(void);
void crazypod_book_session_begin(int book_index);
bool crazypod_book_session_load(int book_index, uint32_t offset);
void crazypod_book_session_set_error(const char *message);
bool crazypod_book_session_turn(int direction);
int crazypod_book_session_index(void);
uint32_t crazypod_book_session_offset(void);
const char *crazypod_book_session_text(void);
bool crazypod_book_session_has_text(void);

#endif
