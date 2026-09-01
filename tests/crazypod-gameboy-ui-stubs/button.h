#include <stdbool.h>
#include <stdint.h>
#define HAS_BUTTON_HOLD
#define HAVE_WHEEL_POSITION
#define BUTTON_NONE 0
#define BUTTON_SELECT 1
#define BUTTON_MENU 2
#define BUTTON_LEFT 4
#define BUTTON_RIGHT 8
#define BUTTON_SCROLL_FWD 16
#define BUTTON_SCROLL_BACK 32
#define BUTTON_PLAY 64
#define BUTTON_REL 0x02000000
#define BUTTON_REPEAT 0x04000000
long button_get(bool block);
int button_status(void);
intptr_t button_get_data(void);
void button_queue_post(long event, intptr_t data);
bool button_hold(void);
int wheel_status(void);
