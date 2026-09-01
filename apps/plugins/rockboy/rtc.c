


#include "rockmacros.h"
#include <stdio.h>

#include "defs.h"
#include "mem.h"
#include "rtc-gb.h"

struct rtc rtc;

void rtc_latch(byte b)
{
    if ((rtc.latch ^ b) & b & 1)
    {
        rtc.regs[0] = rtc.s;
        rtc.regs[1] = rtc.m;
        rtc.regs[2] = rtc.h;
        rtc.regs[3] = rtc.d;
#ifdef CRAZYPOD_GAMEBOY_CORE
        rtc.regs[4] = (rtc.d>>8) | (rtc.stop<<6) | (rtc.carry<<7);
#else
        rtc.regs[4] = (rtc.d>>9) | (rtc.stop<<6) | (rtc.carry<<7);
#endif
        rtc.regs[5] = 0xff;
        rtc.regs[6] = 0xff;
        rtc.regs[7] = 0xff;
    }
    rtc.latch = b;
}

void rtc_write(byte b)
{
    /* printf("write %02X: %02X (%d)\n", rtc.sel, b, b); */
    if (!(rtc.sel & 8)) return;
    switch (rtc.sel & 7)
    {
    case 0:
        rtc.s = rtc.regs[0] = b;
        while (rtc.s >= 60) rtc.s -= 60;
        break;
    case 1:
        rtc.m = rtc.regs[1] = b;
        while (rtc.m >= 60) rtc.m -= 60;
        break;
    case 2:
        rtc.h = rtc.regs[2] = b;
        while (rtc.h >= 24) rtc.h -= 24;
        break;
    case 3:
        rtc.regs[3] = b;
        rtc.d = (rtc.d & 0x100) | b;
        break;
    case 4:
        rtc.regs[4] = b;
#ifdef CRAZYPOD_GAMEBOY_CORE
        rtc.d = (rtc.d & 0xff) | ((b&1)<<8);
#else
        rtc.d = (rtc.d & 0xff) | ((b&1)<<9);
#endif
        rtc.stop = (b>>6)&1;
        rtc.carry = (b>>7)&1;
        break;
    }
}

void rtc_tick(void)
{
    if (rtc.stop) return;
    if (++rtc.t == 60)
    {
        if (++rtc.s == 60)
        {
            if (++rtc.m == 60)
            {
                if (++rtc.h == 24)
                {
#ifdef CRAZYPOD_GAMEBOY_CORE
                    if (++rtc.d == 512)
#else
                    if (++rtc.d == 365)
#endif
                    {
                        rtc.d = 0;
                        rtc.carry = 1;
                    }
                    rtc.h = 0;
                }
                rtc.m = 0;
            }
            rtc.s = 0;
        }
        rtc.t = 0;
    }
}

#ifndef CRAZYPOD_GAMEBOY_CORE
void rtc_save_internal(int fd)
{
    int rt = 0;
    
#if CONFIG_RTC
    rt = rb->mktime(rb->get_time());
#endif
    fdprintf(fd, "%d %d %d %d %d %d %d %d\n", rtc.carry, rtc.stop, rtc.d, rtc.h,
             rtc.m, rtc.s, rtc.t, rt);
}

void rtc_load_internal(int fd)
{
    int rt = 0;
    char buf[32];
    
    rb->read_line(fd, buf, sizeof(buf));
    sscanf(buf, "%d %d %d %d %d %d %d %d\n", &rtc.carry, &rtc.stop, &rtc.d, 
           &rtc.h, &rtc.m, &rtc.s, &rtc.t, &rt);    
    
	while (rtc.t >= 60) rtc.t -= 60;
	while (rtc.s >= 60) rtc.s -= 60;
	while (rtc.m >= 60) rtc.m -= 60;
	while (rtc.h >= 24) rtc.h -= 24;
	while (rtc.d >= 365) rtc.d -= 365;
	rtc.stop &= 1;
	rtc.carry &= 1;

#if CONFIG_RTC    
    if (rt) rt = (rb->mktime(rb->get_time()) - rt) * 60;
#endif
    while (rt-- > 0) rtc_tick();

}
#endif /* CRAZYPOD_GAMEBOY_CORE */












