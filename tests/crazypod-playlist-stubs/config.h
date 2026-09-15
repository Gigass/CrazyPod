#ifndef TEST_CRAZYPOD_PLAYLIST_CONFIG_H
#define TEST_CRAZYPOD_PLAYLIST_CONFIG_H

#define IPOD_6G 1
#define HAVE_CRAZYPOD_UI

/* Enough of the kernel tick API for the repeat-one skip window. */
extern long current_tick;
#define HZ 100
#define TIME_AFTER(a, b) ((long)((b) - (a)) < 0)

#endif
