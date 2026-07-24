#ifndef CRAZYPOD_COVERFLOW_H
#define CRAZYPOD_COVERFLOW_H

#include <stdbool.h>

void crazypod_coverflow_enter(int selected);
void crazypod_coverflow_warm(int selected);
void crazypod_coverflow_leave(void);
bool crazypod_coverflow_active(void);
int crazypod_coverflow_step(int direction);
void crazypod_coverflow_tick(void);

#endif
