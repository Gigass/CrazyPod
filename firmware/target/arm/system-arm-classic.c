/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2006 by Thom Johansen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include "config.h"
#include "system.h"
#include <stdio.h>
#include "lcd.h"
#if defined(HAVE_CRAZYPOD_UI) && !defined(BOOTLOADER)
#include "crazypod/crazypod_lcd.h"
#else
#include "font.h"
#endif
#include "gcc_extensions.h"

#if !defined(HAVE_CRAZYPOD_UI) && !defined(BOOTLOADER)
#include <get_sp.h>
#include <backtrace.h>
#endif

static const char* const uiename[] = {
    "Undefined instruction",
    "Prefetch abort",
    "Data abort",
    "Divide by zero",
    "SWI"
};

void __attribute__((weak,naked)) data_abort_handler(void)
{
    asm volatile(
        "sub    r0, lr, #8  \n"
        "mov    r1, #2      \n"
        "b      UIE         \n"
        );
}

void __attribute__((weak,naked)) software_int_handler(void)
{
    asm volatile(
        "sub    r0, lr, #4  \n"
        "mov    r1, #4      \n"
        "b      UIE         \n"
        );
}

void __attribute__((weak,naked)) reserved_handler(void)
{
    asm volatile(
        "sub    r0, lr, #4  \n"
        "mov    r1, #4      \n"
        "b      UIE         \n"
        );
}

void __attribute__((weak,naked)) prefetch_abort_handler(void)
{
    asm volatile(
        "sub    r0, lr, #4  \n"
        "mov    r1, #1      \n"
        "b      UIE         \n"
        );
}

void __attribute__((weak,naked)) undef_instr_handler(void)
{
    asm volatile(
        "sub    r0, lr, #4    \n"
#ifdef USE_THUMB
        "mrs    r1, spsr      \n"
        "tst    r1, #(1 << 5) \n" // T bit set ?
        "subne  r0, lr, #2    \n" // if yes, offset to THUMB instruction
#endif
        "mov    r1, #0        \n"
        "b      UIE           \n"
        );
}

/* Unexpected Interrupt or Exception handler. Currently only deals with
   exceptions, but will deal with interrupts later.
 */
void NORETURN_ATTR UIE(unsigned int pc, unsigned int num)
{
#if defined(HAVE_CRAZYPOD_UI) && !defined(BOOTLOADER)
    /* Linker-provided bounds: everything executable lives below _edata, and
     * DRAM is mapped up to _end. */
    extern char _edata[];
    extern char _end[];
    extern char stackbegin[];
    char report[256];
    unsigned long spsr = 0;
    unsigned int code[2] = { 0, 0 };
    bool have_code = false;
    unsigned long banked[2] = { 0, 0 };  /* interrupted mode's sp, lr */
    unsigned long code_limit = (unsigned long)_edata;
    unsigned long sp;
    unsigned int frames[4];
    unsigned int nframes = 0;
    int len;
    unsigned int i;

    /* The faulting mode's banked sp/lr still describe the interrupted code,
     * so a wild branch can be traced back to whoever made the call. */
    asm volatile(
        "stmia  %0, {sp, lr}^ \n"
        "nop                  \n"
        : : "r"(banked) : "memory");

    /* SPSR carries the mode and Thumb bit of the interrupted code. Without it
     * the sp/lr above cannot be trusted: they are the User/System banked pair,
     * which is unrelated to the fault if it arrived from another mode. */
    asm volatile("mrs %0, spsr" : "=r"(spsr));

    /* A read is safe where DRAM is mapped, and in the region the main stack
     * lives in, which is how IRAM-resident stacks are reached portably. */
#define CRAZYPOD_PANIC_READABLE(a) \
    ((a) >= 0x100 && \
     ((a) < (unsigned long)_end || \
      ((a) & 0xf0000000ul) == ((unsigned long)stackbegin & 0xf0000000ul)))

    /* The words at the faulting address say what was actually executed. */
    if(CRAZYPOD_PANIC_READABLE((unsigned long)pc)) {
        const volatile unsigned int *at =
            (const volatile unsigned int *)((unsigned long)pc & ~3ul);
        code[0] = at[0];
        code[1] = at[1];
        have_code = true;
    }

    /* Walk the interrupted stack for anything that could be a return
     * address, so a wild branch can be tied back to a caller. */
    sp = banked[0];
    if((sp & 3) == 0 && CRAZYPOD_PANIC_READABLE(sp)) {
        unsigned long addr;
        for(addr = sp; addr + 4 <= sp + 512 && nframes < 4; addr += 4) {
            unsigned long value;

            if(!CRAZYPOD_PANIC_READABLE(addr))
                break;
            value = *(volatile unsigned long *)addr;
            if(value >= 0x100 && value < code_limit && (value & 3) == 0)
                frames[nframes++] = (unsigned int)value;
        }
    }
#undef CRAZYPOD_PANIC_READABLE

    /* On dual-core targets say which core faulted: a fault on the COP
     * points at shared kernel state rather than at the UI thread. */
    len = snprintf(report, sizeof(report),
                   "%s\nPC %08x" IF_COP("\nCORE %d")
                   "\nSPSR %08lx\nLR %08lx\nSP %08lx",
                   uiename[num], pc IF_COP(, CURRENT_CORE),
                   spsr, banked[1], sp);
    if(have_code && len > 0 && (size_t)len < sizeof(report))
        len += snprintf(report + len, sizeof(report) - len,
                        "\nAT %08x %08x", code[0], code[1]);
    for(i = 0; i < nframes && len > 0 && (size_t)len < sizeof(report); i++)
        len += snprintf(report + len, sizeof(report) - len,
                        "%s%08x", i == 0 ? "\nSTACK " : " ", frames[i]);
    crazypod_lcd_show_panic(report);
#else
    /* safe guard variable - we call backtrace() only on first
     * UIE call. This prevent endless loop if backtrace() touches
     * memory regions which cause abort
     */
    static bool triggered = false;

#if LCD_DEPTH > 1
    lcd_set_backdrop(NULL);
    lcd_set_drawinfo(DRMODE_SOLID, LCD_BLACK, LCD_WHITE);
#endif
    unsigned line = 0;

    lcd_setfont(FONT_SYSFIXED);
    lcd_set_viewport(NULL);
    lcd_clear_display();
    lcd_putsf(0, line++, "%s at %08x" IF_COP(" (%d)"), uiename[num], pc IF_COP(, CURRENT_CORE));

#if !defined(CPU_ARM7TDMI) && (CONFIG_CPU != RK27XX) /* arm7tdmi has no MPU/MMU */
    if(num == 1 || num == 2) /* prefetch / data abort */
    {
        register unsigned status;

#if ARM_ARCH >= 6
        /* ARMv6 has 2 different registers for prefetch & data aborts */
        if(num == 1)    /* instruction prefetch abort */
            asm volatile( "mrc p15, 0, %0, c5, c0, 1\n" : "=r"(status));
        else
#endif
            asm volatile( "mrc p15, 0, %0, c5, c0, 0\n" : "=r"(status));

        lcd_putsf(0, line++, "FSR 0x%x", status);

        unsigned int domain = (status >> 4) & 0xf;
        unsigned int fault = status & 0xf;
#if ARM_ARCH >= 6
        fault |= (status & (1<<10)) >> 6; /* fault is 5 bits on armv6 */
#endif
        lcd_putsf(0, line++, "(domain %d, fault %d)", domain, fault);

        if(num == 2) /* data abort */
        {
            register unsigned address;
            /* read FAR (fault address register) */
            asm volatile( "mrc p15, 0, %0, c6, c0\n" : "=r"(address));
            lcd_putsf(0, line++, "address 0x%8x", address);
#if ARM_ARCH >= 6
            lcd_putsf(0, line++, (status & (1<<11)) ? "(write)" : "(read)");
#endif
        }
    }   /* num == 1 || num == 2 // prefetch/data abort */
#endif /* !defined(CPU_ARM7TDMI */

#if defined(HAVE_RB_BACKTRACE) && !defined(BOOTLOADER)
    if (!triggered)
    {
        triggered = true;
        rb_backtrace(pc, __get_sp(), &line);
    }
#else
    (void)triggered;
#endif

    lcd_update();
#endif

    disable_interrupt(IRQ_FIQ_STATUS);

    system_exception_wait(); /* If this returns, try to reboot */
    system_reboot();
    while (1);       /* halt */
}

/* Needs to be here or gcc won't find it */
void __attribute__((naked)) __div0(void)
{
    asm volatile (
        "ldr    r0, [sp]    \r\n"
        "sub    r0, r0, #4  \r\n"
        "mov    r1, #3      \r\n"
        "b      UIE         \r\n"
    );
}
