#ifndef TEST_CRAZYPOD_IAP_SYSTEM_H
#define TEST_CRAZYPOD_IAP_SYSTEM_H

static inline int disable_irq_save(void)
{
    return 0;
}

static inline void restore_irq(int level)
{
    (void)level;
}

#endif
