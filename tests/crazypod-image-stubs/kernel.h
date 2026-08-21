#ifndef CRAZYPOD_IMAGE_TEST_KERNEL_H
#define CRAZYPOD_IMAGE_TEST_KERNEL_H

struct mutex {
    int unused;
};

static inline void mutex_init(struct mutex *mutex)
{
    (void)mutex;
}

static inline void mutex_lock(struct mutex *mutex)
{
    (void)mutex;
}

static inline void mutex_unlock(struct mutex *mutex)
{
    (void)mutex;
}

#endif
