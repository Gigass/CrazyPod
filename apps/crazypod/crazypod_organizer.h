#ifndef CRAZYPOD_ORGANIZER_H
#define CRAZYPOD_ORGANIZER_H

#include <stdbool.h>
#include <stdint.h>

struct crazypod_contact {
    char name[96];
    char phone[64];
    char email[96];
};

struct crazypod_calendar_event {
    uint32_t id;
    int date;
    char time[16];
    char summary[96];
    bool editable;
};

void crazypod_organizer_scan(void);
int crazypod_contacts_count(void);
const struct crazypod_contact *crazypod_contact_get(int index);
int crazypod_calendar_event_count(void);
const struct crazypod_calendar_event *crazypod_calendar_event_get(int index);
const struct crazypod_calendar_event *crazypod_calendar_event_find(
    uint32_t id);
uint32_t crazypod_calendar_event_add(int date, const char *time,
                                     const char *summary);
bool crazypod_calendar_event_update(uint32_t id, int date,
                                    const char *time,
                                    const char *summary);
bool crazypod_calendar_event_delete(uint32_t id);

#endif
