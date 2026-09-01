#define HZ 100
extern long current_tick;
#define TIME_BEFORE(a,b) ((long)((a)-(b)) < 0)
#define TIME_AFTER(a,b) TIME_BEFORE(b,a)
void sleep(int ticks);
void yield(void);
