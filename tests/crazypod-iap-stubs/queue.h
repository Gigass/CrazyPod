#ifndef TEST_CRAZYPOD_IAP_QUEUE_H
#define TEST_CRAZYPOD_IAP_QUEUE_H

#define SYS_EVENT_CLS_PRIVATE 0x35
#define MAKE_SYS_EVENT(cls, id) \
    ((long)(0x40000000u | ((unsigned long)(cls) << 8) | (id)))

#endif
