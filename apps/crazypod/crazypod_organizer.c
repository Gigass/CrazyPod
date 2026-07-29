#include "config.h"

#ifdef IPOD_6G

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dir.h"
#include "file.h"

#include "crazypod_organizer.h"

#define CONTACTS_MAX 64
#define EVENTS_MAX 128
#define LOCAL_EVENTS_MAX 64
#define ORGANIZER_LINE_SIZE 384
#define ORGANIZER_DIRECTORY "/.crazypod"
#define CALENDAR_PATH ORGANIZER_DIRECTORY "/calendar.bin"
#define CALENDAR_TEMP_PATH ORGANIZER_DIRECTORY "/calendar.tmp"
#define CALENDAR_MAGIC 0x43504341u
#define CALENDAR_VERSION 1u

struct calendar_event_disk {
    uint32_t id;
    int32_t date;
    char time[16];
    char summary[96];
};

struct calendar_disk {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t count;
    struct calendar_event_disk events[LOCAL_EVENTS_MAX];
    uint32_t checksum;
};

static struct crazypod_contact contacts[CONTACTS_MAX];
static struct crazypod_calendar_event events[EVENTS_MAX];
static struct calendar_disk calendar_save_buffer;
static int contact_count;
static int event_count;
static uint32_t next_event_id = 1;
static bool organizer_loaded;
static bool organizer_dirty = true;

static uint32_t hash_bytes(uint32_t hash, const void *data, size_t size)
{
    const unsigned char *bytes = data;
    size_t i;

    for(i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t calendar_checksum(const struct calendar_disk *disk)
{
    return hash_bytes(2166136261u, disk,
                      offsetof(struct calendar_disk, checksum));
}

static bool read_exact(int fd, void *buffer, size_t size)
{
    unsigned char *bytes = buffer;
    size_t done = 0;

    while(done < size) {
        ssize_t count = read(fd, bytes + done, size - done);
        if(count <= 0)
            return false;
        done += (size_t)count;
    }
    return true;
}

static bool write_exact(int fd, const void *buffer, size_t size)
{
    const unsigned char *bytes = buffer;
    size_t done = 0;

    while(done < size) {
        ssize_t count = write(fd, bytes + done, size - done);
        if(count <= 0)
            return false;
        done += (size_t)count;
    }
    return true;
}

static bool ends_with_ignore_case(const char *text, const char *suffix)
{
    size_t text_length = strlen(text);
    size_t suffix_length = strlen(suffix);
    size_t i;
    if(text_length < suffix_length)
        return false;
    text += text_length - suffix_length;
    for(i = 0; i < suffix_length; ++i) {
        char left = text[i];
        char right = suffix[i];
        if(left >= 'A' && left <= 'Z')
            left = (char)(left - 'A' + 'a');
        if(right >= 'A' && right <= 'Z')
            right = (char)(right - 'A' + 'a');
        if(left != right)
            return false;
    }
    return true;
}

static int read_line(int fd, char *line, size_t size)
{
    size_t length = 0;
    char value;
    int result;
    while((result = read(fd, &value, 1)) == 1) {
        if(value == '\n')
            break;
        if(value == '\r')
            continue;
        if(length + 1 < size)
            line[length++] = value;
    }
    line[length] = '\0';
    return result == 1 || length > 0 ? (int)length : -1;
}

static int read_unfolded_line(int fd, char *line, size_t size,
                              char *pending, bool *has_pending)
{
    char next[ORGANIZER_LINE_SIZE];
    size_t length;

    if(*has_pending) {
        snprintf(line, size, "%s", pending);
        *has_pending = false;
    }
    else if(read_line(fd, line, size) < 0)
        return -1;

    length = strlen(line);
    while(read_line(fd, next, sizeof(next)) >= 0) {
        if(next[0] == ' ' || next[0] == '\t') {
            size_t continuation = strlen(next + 1);
            if(length + continuation + 1 > size)
                continuation = size - length - 1;
            memcpy(line + length, next + 1, continuation);
            length += continuation;
            line[length] = '\0';
        }
        else {
            snprintf(pending, ORGANIZER_LINE_SIZE, "%s", next);
            *has_pending = true;
            break;
        }
    }
    return (int)length;
}

static const char *property_value(const char *line, const char *name)
{
    const char *colon;
    size_t length = strlen(name);
    if(strncmp(line, name, length) != 0)
        return NULL;
    if(line[length] != ':' && line[length] != ';')
        return NULL;
    colon = strchr(line + length, ':');
    return colon != NULL ? colon + 1 : NULL;
}

static void copy_text(char *destination, size_t size, const char *source)
{
    snprintf(destination, size, "%s", source != NULL ? source : "");
}

static void copy_unescaped_text(char *destination, size_t size,
                                const char *source)
{
    size_t output = 0;

    if(size == 0)
        return;
    while(source != NULL && *source != '\0' && output + 1 < size) {
        char value = *source++;
        if(value == '\\' && *source != '\0') {
            value = *source++;
            if(value == 'n' || value == 'N')
                value = ' ';
        }
        destination[output++] = value;
    }
    destination[output] = '\0';
}

static bool valid_date(int date)
{
    static const int days_per_month[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };
    int year = date / 10000;
    int month = date / 100 % 100;
    int day = date % 100;
    int maximum;

    if(year < 1970 || year > 2199 ||
       month < 1 || month > 12 || day < 1)
        return false;
    maximum = days_per_month[month - 1];
    if(month == 2 &&
       ((year % 4 == 0 && year % 100 != 0) ||
        year % 400 == 0))
        maximum = 29;
    return day <= maximum;
}

static int compare_event(const struct crazypod_calendar_event *left,
                         const struct crazypod_calendar_event *right)
{
    int result;

    if(left->date != right->date)
        return left->date < right->date ? -1 : 1;
    result = strcmp(left->time, right->time);
    if(result != 0)
        return result;
    return strcmp(left->summary, right->summary);
}

static void sort_events(void)
{
    int i;

    for(i = 1; i < event_count; ++i) {
        struct crazypod_calendar_event value = events[i];
        int position = i;
        while(position > 0 &&
              compare_event(&value, &events[position - 1]) < 0) {
            events[position] = events[position - 1];
            --position;
        }
        events[position] = value;
    }
}

static void load_local_events(void)
{
    static struct calendar_disk disk;
    int fd = open(CALENDAR_PATH, O_RDONLY);
    uint32_t i;

    if(fd < 0)
        return;
    if(!read_exact(fd, &disk, sizeof(disk)) ||
       disk.magic != CALENDAR_MAGIC ||
       disk.version != CALENDAR_VERSION ||
       disk.size != sizeof(disk) ||
       disk.count > LOCAL_EVENTS_MAX ||
       disk.checksum != calendar_checksum(&disk)) {
        close(fd);
        return;
    }
    close(fd);

    for(i = 0; i < disk.count && event_count < EVENTS_MAX; ++i) {
        struct crazypod_calendar_event *event;
        const struct calendar_event_disk *stored = &disk.events[i];

        if(stored->id == 0 || !valid_date(stored->date))
            continue;
        event = &events[event_count++];
        memset(event, 0, sizeof(*event));
        event->id = stored->id;
        event->date = stored->date;
        copy_text(event->time, sizeof(event->time), stored->time);
        copy_text(event->summary, sizeof(event->summary),
                  stored->summary[0] != '\0'
                      ? stored->summary : "Calendar Event");
        event->editable = true;
        if(event->id >= next_event_id)
            next_event_id = event->id + 1;
    }
}

static bool save_local_events(void)
{
    struct calendar_disk *disk = &calendar_save_buffer;
    int fd;
    int i;
    bool success;

    memset(disk, 0, sizeof(*disk));
    disk->magic = CALENDAR_MAGIC;
    disk->version = CALENDAR_VERSION;
    disk->size = sizeof(*disk);
    for(i = 0; i < event_count &&
               disk->count < LOCAL_EVENTS_MAX; ++i) {
        struct calendar_event_disk *stored;
        if(!events[i].editable || events[i].id == 0)
            continue;
        stored = &disk->events[disk->count++];
        stored->id = events[i].id;
        stored->date = events[i].date;
        copy_text(stored->time, sizeof(stored->time), events[i].time);
        copy_text(stored->summary, sizeof(stored->summary),
                  events[i].summary);
    }
    disk->checksum = calendar_checksum(disk);

    mkdir(ORGANIZER_DIRECTORY);
    fd = open(CALENDAR_TEMP_PATH,
              O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd < 0)
        return false;
    success = write_exact(fd, disk, sizeof(*disk));
    if(fsync(fd) < 0)
        success = false;
    close(fd);
    return success && rename(CALENDAR_TEMP_PATH, CALENDAR_PATH) == 0;
}

static void parse_vcard(const char *path)
{
    struct crazypod_contact current;
    char line[ORGANIZER_LINE_SIZE];
    char pending[ORGANIZER_LINE_SIZE];
    bool active = false;
    bool has_pending = false;
    int fd = open(path, O_RDONLY);

    if(fd < 0)
        return;
    memset(&current, 0, sizeof(current));
    while(read_unfolded_line(fd, line, sizeof(line),
                             pending, &has_pending) >= 0 &&
          contact_count < CONTACTS_MAX) {
        const char *value;
        if(strlen(line) >= 3 &&
           (unsigned char)line[0] == 0xef &&
           (unsigned char)line[1] == 0xbb &&
           (unsigned char)line[2] == 0xbf)
            memmove(line, line + 3, strlen(line + 3) + 1);
        if(strcmp(line, "BEGIN:VCARD") == 0) {
            memset(&current, 0, sizeof(current));
            active = true;
        }
        else if(active && strcmp(line, "END:VCARD") == 0) {
            if(current.name[0] == '\0')
                copy_text(current.name, sizeof(current.name),
                          current.phone[0] != '\0'
                              ? current.phone : "Unnamed Contact");
            contacts[contact_count++] = current;
            active = false;
        }
        else if(active &&
                (value = property_value(line, "FN")) != NULL)
            copy_unescaped_text(current.name, sizeof(current.name), value);
        else if(active && current.phone[0] == '\0' &&
                (value = property_value(line, "TEL")) != NULL)
            copy_unescaped_text(current.phone, sizeof(current.phone), value);
        else if(active && current.email[0] == '\0' &&
                (value = property_value(line, "EMAIL")) != NULL)
            copy_unescaped_text(current.email, sizeof(current.email), value);
    }
    close(fd);
}

static int parse_date(const char *value, char *time, size_t time_size)
{
    int year;
    int month;
    int day;
    if(value == NULL || strlen(value) < 8)
        return 0;
    year = (value[0] - '0') * 1000 + (value[1] - '0') * 100 +
           (value[2] - '0') * 10 + value[3] - '0';
    month = (value[4] - '0') * 10 + value[5] - '0';
    day = (value[6] - '0') * 10 + value[7] - '0';
    time[0] = '\0';
    if(strlen(value) >= 13 && value[8] == 'T') {
        snprintf(time, time_size, "%c%c:%c%c",
                 value[9], value[10], value[11], value[12]);
    }
    return year * 10000 + month * 100 + day;
}

static void parse_ics(const char *path)
{
    struct crazypod_calendar_event current;
    char line[ORGANIZER_LINE_SIZE];
    char pending[ORGANIZER_LINE_SIZE];
    bool active = false;
    bool has_pending = false;
    int fd = open(path, O_RDONLY);

    if(fd < 0)
        return;
    memset(&current, 0, sizeof(current));
    while(read_unfolded_line(fd, line, sizeof(line),
                             pending, &has_pending) >= 0 &&
          event_count < EVENTS_MAX) {
        const char *value;
        if(strlen(line) >= 3 &&
           (unsigned char)line[0] == 0xef &&
           (unsigned char)line[1] == 0xbb &&
           (unsigned char)line[2] == 0xbf)
            memmove(line, line + 3, strlen(line + 3) + 1);
        if(strcmp(line, "BEGIN:VEVENT") == 0) {
            memset(&current, 0, sizeof(current));
            active = true;
        }
        else if(active && strcmp(line, "END:VEVENT") == 0) {
            if(valid_date(current.date)) {
                if(current.summary[0] == '\0')
                    copy_text(current.summary, sizeof(current.summary),
                              "Calendar Event");
                current.id = 0;
                current.editable = false;
                events[event_count++] = current;
            }
            active = false;
        }
        else if(active &&
                (value = property_value(line, "DTSTART")) != NULL)
            current.date = parse_date(value, current.time,
                                      sizeof(current.time));
        else if(active &&
                (value = property_value(line, "SUMMARY")) != NULL)
            copy_unescaped_text(current.summary,
                                sizeof(current.summary), value);
    }
    close(fd);
}

static void scan_directory(const char *path, const char *suffix,
                           void (*parse)(const char *))
{
    DIR *directory = opendir(path);
    struct DIRENT *entry;
    if(directory == NULL)
        return;
    while((entry = readdir(directory)) != NULL) {
        char child[MAX_PATH];
        struct dirinfo info;
        int result;
        if(entry->d_name[0] == '.')
            continue;
        result = snprintf(child, sizeof(child), "%s/%s",
                          path, entry->d_name);
        if(result <= 0 || result >= (int)sizeof(child))
            continue;
        info = dir_get_info(directory, entry);
        if(!(info.attribute & ATTR_DIRECTORY) &&
           ends_with_ignore_case(child, suffix))
            parse(child);
    }
    closedir(directory);
}

void crazypod_organizer_scan(void)
{
    contact_count = 0;
    event_count = 0;
    next_event_id = 1;
    memset(contacts, 0, sizeof(contacts));
    memset(events, 0, sizeof(events));
    load_local_events();
    scan_directory("/Contacts", ".vcf", parse_vcard);
    scan_directory("/Calendars", ".ics", parse_ics);
    scan_directory("/Calendar", ".ics", parse_ics);
    sort_events();
    organizer_loaded = true;
    organizer_dirty = false;
}

void crazypod_organizer_ensure_loaded(void)
{
    if(!organizer_loaded || organizer_dirty)
        crazypod_organizer_scan();
}

void crazypod_organizer_invalidate(void)
{
    organizer_dirty = true;
}

int crazypod_contacts_count(void)
{
    crazypod_organizer_ensure_loaded();
    return contact_count;
}

const struct crazypod_contact *crazypod_contact_get(int index)
{
    crazypod_organizer_ensure_loaded();
    return index >= 0 && index < contact_count ? &contacts[index] : NULL;
}

int crazypod_calendar_event_count(void)
{
    crazypod_organizer_ensure_loaded();
    return event_count;
}

const struct crazypod_calendar_event *crazypod_calendar_event_get(int index)
{
    crazypod_organizer_ensure_loaded();
    return index >= 0 && index < event_count ? &events[index] : NULL;
}

const struct crazypod_calendar_event *crazypod_calendar_event_find(
    uint32_t id)
{
    int i;

    crazypod_organizer_ensure_loaded();
    if(id == 0)
        return NULL;
    for(i = 0; i < event_count; ++i)
        if(events[i].id == id)
            return &events[i];
    return NULL;
}

uint32_t crazypod_calendar_event_add(int date, const char *time,
                                     const char *summary)
{
    struct crazypod_calendar_event *event;
    uint32_t id;
    int local_count = 0;
    int i;

    crazypod_organizer_ensure_loaded();
    if(!valid_date(date) || summary == NULL || summary[0] == '\0' ||
       event_count >= EVENTS_MAX)
        return 0;
    for(i = 0; i < event_count; ++i)
        if(events[i].editable)
            ++local_count;
    if(local_count >= LOCAL_EVENTS_MAX)
        return 0;
    if(next_event_id == 0)
        next_event_id = 1;

    event = &events[event_count++];
    memset(event, 0, sizeof(*event));
    id = next_event_id++;
    event->id = id;
    event->date = date;
    event->editable = true;
    copy_text(event->time, sizeof(event->time), time);
    copy_text(event->summary, sizeof(event->summary), summary);
    sort_events();
    if(!save_local_events()) {
        for(i = 0; i < event_count; ++i) {
            if(events[i].id == id) {
                memmove(&events[i], &events[i + 1],
                        (size_t)(event_count - i - 1) *
                            sizeof(events[0]));
                --event_count;
                break;
            }
        }
        return 0;
    }
    return id;
}

bool crazypod_calendar_event_update(uint32_t id, int date,
                                    const char *time,
                                    const char *summary)
{
    int i;

    crazypod_organizer_ensure_loaded();
    if(id == 0 || !valid_date(date) ||
       summary == NULL || summary[0] == '\0')
        return false;
    for(i = 0; i < event_count; ++i) {
        struct crazypod_calendar_event before;
        if(events[i].id != id || !events[i].editable)
            continue;
        before = events[i];
        events[i].date = date;
        copy_text(events[i].time, sizeof(events[i].time), time);
        copy_text(events[i].summary, sizeof(events[i].summary), summary);
        sort_events();
        if(save_local_events())
            return true;
        for(i = 0; i < event_count; ++i) {
            if(events[i].id == id) {
                events[i] = before;
                sort_events();
                break;
            }
        }
        return false;
    }
    return false;
}

bool crazypod_calendar_event_delete(uint32_t id)
{
    int i;

    crazypod_organizer_ensure_loaded();
    if(id == 0)
        return false;
    for(i = 0; i < event_count; ++i) {
        struct crazypod_calendar_event removed;
        if(events[i].id != id || !events[i].editable)
            continue;
        removed = events[i];
        memmove(&events[i], &events[i + 1],
                (size_t)(event_count - i - 1) *
                    sizeof(events[0]));
        --event_count;
        if(save_local_events())
            return true;
        events[event_count++] = removed;
        sort_events();
        return false;
    }
    return false;
}

#endif
