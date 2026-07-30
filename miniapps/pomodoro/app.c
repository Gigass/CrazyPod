#include "engine.h"
#include "../sdk/crazypod_miniapp.h"
#include "../sdk/crazypod_miniapp_l10n.h"

#define POMODORO_SETUP_ITEM_COUNT 5u

enum pomodoro_setup_item {
    POMODORO_SETUP_FOCUS = 0,
    POMODORO_SETUP_SHORT,
    POMODORO_SETUP_LONG,
    POMODORO_SETUP_ROUNDS,
    POMODORO_SETUP_DONE
};

static const struct cp_host_api *pomodoro_host;
static struct pomodoro_model pomodoro_app_model;
static struct pomodoro_config pomodoro_setup_config;
static uint8_t pomodoro_action_index;
static uint8_t pomodoro_setup_index;
static bool pomodoro_setup_active;
static bool pomodoro_setup_editing;
static bool pomodoro_storage_error;
static bool pomodoro_alarm_error;
static uint32_t pomodoro_last_render_remaining;
static uint32_t pomodoro_language;

static const char *tr(const char *source)
{
    return cp_l10n_text(pomodoro_language, source);
}

static void append_character(char *buffer, size_t capacity, size_t *length,
                             char character)
{
    if(buffer == NULL || length == NULL || capacity == 0)
        return;
    if(*length + 1u < capacity) {
        buffer[*length] = character;
        ++*length;
        buffer[*length] = '\0';
    }
}

static void append_text(char *buffer, size_t capacity, size_t *length,
                        const char *text)
{
    size_t index = 0;

    if(text == NULL)
        return;
    while(text[index] != '\0') {
        append_character(buffer, capacity, length, text[index]);
        ++index;
    }
}

static void append_unsigned(char *buffer, size_t capacity, size_t *length,
                            uint32_t value)
{
    char digits[10];
    size_t count = 0;

    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while(value != 0 && count < sizeof(digits));
    while(count > 0)
        append_character(buffer, capacity, length, digits[--count]);
}

static void format_countdown(uint32_t seconds, char *buffer, size_t capacity)
{
    uint32_t minutes = seconds / 60u;
    uint32_t remainder = seconds % 60u;
    size_t length = 0;

    if(capacity == 0)
        return;
    if(CP_HOST_HAS(pomodoro_host, CP_CAP_FORMAT_DURATION,
                   format_duration)) {
        pomodoro_host->format_duration(seconds, buffer, capacity);
        return;
    }
    buffer[0] = '\0';
    append_unsigned(buffer, capacity, &length, minutes);
    append_character(buffer, capacity, &length, ':');
    if(remainder < 10u)
        append_character(buffer, capacity, &length, '0');
    append_unsigned(buffer, capacity, &length, remainder);
}

static void format_round(const struct pomodoro_model *model,
                         char *buffer, size_t capacity)
{
    size_t length = 0;

    if(capacity == 0)
        return;
    buffer[0] = '\0';
    append_unsigned(buffer, capacity, &length, pomodoro_round_number(model));
    append_text(buffer, capacity, &length, tr(" OF "));
    append_unsigned(buffer, capacity, &length, model->config.rounds);
}

static void format_setting(uint16_t value, bool minutes, bool editing,
                           char *buffer, size_t capacity)
{
    size_t length = 0;

    if(capacity == 0)
        return;
    buffer[0] = '\0';
    if(editing)
        append_text(buffer, capacity, &length, "< ");
    append_unsigned(buffer, capacity, &length, value);
    append_text(buffer, capacity, &length,
                tr(minutes ? " MIN" : " ROUNDS"));
    if(editing)
        append_text(buffer, capacity, &length, " >");
}

static const char *phase_name(enum pomodoro_phase phase)
{
    if(phase == POMODORO_PHASE_SHORT_BREAK)
        return tr("SHORT BREAK");
    if(phase == POMODORO_PHASE_LONG_BREAK)
        return tr("LONG BREAK");
    return tr("FOCUS");
}

static const char *short_phase_name(enum pomodoro_phase phase)
{
    if(phase == POMODORO_PHASE_SHORT_BREAK)
        return tr("SHORT");
    if(phase == POMODORO_PHASE_LONG_BREAK)
        return tr("LONG");
    return tr("FOCUS");
}

static const char *state_name(enum pomodoro_run_state state)
{
    if(state == POMODORO_RUNNING)
        return tr("RUNNING");
    if(state == POMODORO_PAUSED)
        return tr("PAUSED");
    if(state == POMODORO_COMPLETE)
        return tr("COMPLETE");
    return tr("READY");
}

static enum cp_color_token phase_color(enum pomodoro_phase phase)
{
    if(phase == POMODORO_PHASE_SHORT_BREAK)
        return CP_COLOR_GREEN;
    if(phase == POMODORO_PHASE_LONG_BREAK)
        return CP_COLOR_CYAN;
    return CP_COLOR_ROSE;
}

static struct cp_draw_command *
add_rect(struct cp_scene *scene, int x, int y, int width, int height,
         enum cp_color_token background, int radius)
{
    struct cp_draw_command *command =
        cp_scene_add(scene, CP_DRAW_RECT);

    if(command == NULL)
        return NULL;
    command->x = (int16_t)x;
    command->y = (int16_t)y;
    command->width = (int16_t)width;
    command->height = (int16_t)height;
    command->background = (uint8_t)background;
    command->radius = (uint8_t)radius;
    return command;
}

static struct cp_draw_command *
add_text(struct cp_scene *scene, int x, int y, int width, int height,
         enum cp_font_token font, enum cp_text_align align,
         enum cp_color_token foreground, const char *text)
{
    struct cp_draw_command *command =
        cp_scene_add(scene, CP_DRAW_TEXT);

    if(command == NULL)
        return NULL;
    command->x = (int16_t)x;
    command->y = (int16_t)y;
    command->width = (int16_t)width;
    command->height = (int16_t)height;
    command->font = (uint8_t)font;
    command->align = (uint8_t)align;
    command->foreground = (uint8_t)foreground;
    cp_text_copy(command->text, sizeof(command->text), text);
    return command;
}

static unsigned timer_action_count(void)
{
    if(pomodoro_app_model.run_state == POMODORO_RUNNING ||
       pomodoro_app_model.run_state == POMODORO_PAUSED)
        return 3;
    return 2;
}

static const char *timer_action_name(unsigned index)
{
    enum pomodoro_run_state state =
        (enum pomodoro_run_state)pomodoro_app_model.run_state;

    if(state == POMODORO_READY)
        return tr(index == 0 ? "START" : "SETUP");
    if(state == POMODORO_RUNNING) {
        if(index == 0)
            return tr("PAUSE");
        return tr(index == 1 ? "SKIP" : "RESET");
    }
    if(state == POMODORO_PAUSED) {
        if(index == 0)
            return tr("RESUME");
        return tr(index == 1 ? "SKIP" : "RESET");
    }
    return tr(index == 0 ? "NEXT" : "RESET");
}

static void render_action(struct cp_scene *scene, int y, unsigned index)
{
    bool focused = index == pomodoro_action_index;
    struct cp_draw_command *command =
        add_rect(scene, 188, y, 104, 28,
                 focused ? CP_COLOR_ACCENT : CP_COLOR_SURFACE_RAISED, 7);

    if(command != NULL) {
        command->flags = focused ? CP_DRAW_FOCUSED : 0;
        command->border = focused ? CP_COLOR_ACCENT : CP_COLOR_MUTED;
        command->border_width = 1;
        command->border_opacity = focused ? 255 : 96;
    }
    add_text(scene, 188, y + 6, 104, 16, CP_FONT_LABEL, CP_ALIGN_CENTER,
             focused ? CP_COLOR_ACCENT_FOREGROUND : CP_COLOR_WHITE,
             timer_action_name(index));
}

static void render_timer(struct cp_scene *scene)
{
    struct cp_draw_command *ring;
    struct cp_draw_command *progress;
    enum pomodoro_phase phase =
        (enum pomodoro_phase)pomodoro_app_model.phase;
    enum cp_color_token color = phase_color(phase);
    uint32_t remaining = pomodoro_remaining_at(
        &pomodoro_app_model, pomodoro_host->epoch_seconds());
    uint32_t duration = pomodoro_phase_duration(&pomodoro_app_model);
    uint32_t elapsed = duration >= remaining ? duration - remaining : 0;
    unsigned action_count = timer_action_count();
    int action_y = action_count == 3 ? 126 : 142;
    char text[CP_MINIAPP_TEXT_SIZE];
    size_t length = 0;
    unsigned index;

    add_rect(scene, 10, 40, 300, 188, CP_COLOR_SURFACE, 12);
    ring = cp_scene_add(scene, CP_DRAW_RING);
    if(ring != NULL) {
        ring->x = 22;
        ring->y = 54;
        ring->width = 152;
        ring->height = 152;
        ring->track_color = CP_COLOR_SURFACE_RAISED;
        ring->progress_color = (uint8_t)color;
        ring->track_width = 5;
        ring->progress_width = 7;
        ring->value = (int32_t)elapsed;
        ring->maximum = (int32_t)duration;
    }

    format_countdown(remaining, text, sizeof(text));
    add_text(scene, 28, 99, 140, 50, CP_FONT_DISPLAY, CP_ALIGN_CENTER,
             CP_COLOR_WHITE, text);
    add_text(scene, 43, 153, 110, 16, CP_FONT_CAPTION, CP_ALIGN_CENTER,
             color, state_name((enum pomodoro_run_state)
                               pomodoro_app_model.run_state));
    if((pomodoro_host->capabilities & CP_CAP_DRAW_PROGRESS) != 0) {
        progress = cp_scene_add(scene, CP_DRAW_PROGRESS);
        if(progress != NULL) {
            progress->x = 43;
            progress->y = 174;
            progress->width = 110;
            progress->height = 5;
            progress->radius = 3;
            progress->track_color = CP_COLOR_SURFACE_RAISED;
            progress->progress_color = (uint8_t)color;
            progress->value = (int32_t)elapsed;
            progress->maximum = (int32_t)duration;
        }
    }

    add_text(scene, 188, 56, 104, 16, CP_FONT_LABEL, CP_ALIGN_LEFT,
             color, phase_name(phase));
    format_round(&pomodoro_app_model, text, sizeof(text));
    add_text(scene, 188, 74, 104, 14, CP_FONT_CAPTION, CP_ALIGN_LEFT,
             CP_COLOR_MUTED, text);
    add_text(scene, 188, 91, 104, 14, CP_FONT_LABEL, CP_ALIGN_LEFT,
             CP_COLOR_WHITE,
             state_name((enum pomodoro_run_state)
                        pomodoro_app_model.run_state));
    text[0] = '\0';
    append_text(text, sizeof(text), &length, tr("NEXT"));
    append_text(text, sizeof(text), &length, " ");
    append_text(text, sizeof(text), &length,
                short_phase_name(pomodoro_next_phase(&pomodoro_app_model)));
    add_text(scene, 188, 107, 104, 14, CP_FONT_CAPTION, CP_ALIGN_LEFT,
             CP_COLOR_MUTED, text);

    if(pomodoro_storage_error || pomodoro_alarm_error) {
        add_text(scene, 22, 207, 152, 14, CP_FONT_CAPTION, CP_ALIGN_CENTER,
                 CP_COLOR_ERROR,
                 pomodoro_alarm_error
                     ? tr("TIMER NOT STARTED")
                     : tr("CHANGES NOT SAVED"));
    }

    if(pomodoro_action_index >= action_count)
        pomodoro_action_index = 0;
    for(index = 0; index < action_count; ++index)
        render_action(scene, action_y + (int)index * 32, index);
}

static const char *setup_label(unsigned index)
{
    if(index == POMODORO_SETUP_FOCUS)
        return tr("FOCUS");
    if(index == POMODORO_SETUP_SHORT)
        return tr("SHORT BREAK");
    if(index == POMODORO_SETUP_LONG)
        return tr("LONG BREAK");
    if(index == POMODORO_SETUP_ROUNDS)
        return tr("ROUNDS");
    return tr("DONE");
}

static uint16_t setup_value(unsigned index)
{
    if(index == POMODORO_SETUP_FOCUS)
        return pomodoro_setup_config.focus_minutes;
    if(index == POMODORO_SETUP_SHORT)
        return pomodoro_setup_config.short_break_minutes;
    if(index == POMODORO_SETUP_LONG)
        return pomodoro_setup_config.long_break_minutes;
    return pomodoro_setup_config.rounds;
}

static void render_setup(struct cp_scene *scene)
{
    unsigned index;

    add_rect(scene, 10, 40, 300, 188, CP_COLOR_SURFACE, 12);
    for(index = 0; index < POMODORO_SETUP_ITEM_COUNT; ++index) {
        int y = 50 + (int)index * 34;
        bool focused = index == pomodoro_setup_index;
        struct cp_draw_command *row =
            add_rect(scene, 20, y, 280, 28,
                     focused ? CP_COLOR_ACCENT :
                               CP_COLOR_SURFACE_RAISED, 7);

        if(row != NULL) {
            row->flags = focused ? CP_DRAW_FOCUSED : 0;
            row->border = focused ? CP_COLOR_ACCENT : CP_COLOR_MUTED;
            row->border_width = 1;
            row->border_opacity = focused ? 255 : 72;
        }
        if(index == POMODORO_SETUP_DONE) {
            add_text(scene, 20, y + 6, 280, 16, CP_FONT_LABEL,
                     CP_ALIGN_CENTER,
                     focused ? CP_COLOR_ACCENT_FOREGROUND :
                               CP_COLOR_WHITE,
                     setup_label(index));
        }
        else {
            char value[CP_MINIAPP_TEXT_SIZE];

            add_text(scene, 32, y + 6, 132, 16, CP_FONT_LABEL,
                     CP_ALIGN_LEFT,
                     focused ? CP_COLOR_ACCENT_FOREGROUND :
                               CP_COLOR_WHITE,
                     setup_label(index));
            format_setting(setup_value(index),
                           index != POMODORO_SETUP_ROUNDS,
                           focused && pomodoro_setup_editing,
                           value, sizeof(value));
            add_text(scene, 164, y + 6, 124, 16, CP_FONT_LABEL,
                     CP_ALIGN_RIGHT,
                     focused ? CP_COLOR_ACCENT_FOREGROUND :
                               CP_COLOR_MUTED,
                     value);
        }
    }
}

static bool persist_model(void)
{
    struct pomodoro_disk_state disk;
    int result;

    pomodoro_pack(&pomodoro_app_model, &disk);
    result = pomodoro_host->state_write(&disk, sizeof(disk));
    pomodoro_storage_error = result < 0;
    return !pomodoro_storage_error;
}

static void acknowledge_alarm(void)
{
    uint32_t token;

    if(pomodoro_host->alarm_fired(&token))
        pomodoro_host->alarm_acknowledge();
}

static bool poll_alarm(void)
{
    uint32_t token;
    bool changed = false;

    if(!pomodoro_host->alarm_fired(&token))
        return false;
    changed = pomodoro_alarm_fired(&pomodoro_app_model, token);
    pomodoro_host->alarm_acknowledge();
    if(changed)
        persist_model();
    return changed;
}

static bool start_or_resume(void)
{
    struct pomodoro_model previous = pomodoro_app_model;
    uint32_t now = pomodoro_host->epoch_seconds();
    bool changed;

    if(pomodoro_app_model.run_state == POMODORO_READY)
        changed = pomodoro_start(&pomodoro_app_model, now);
    else
        changed = pomodoro_resume(&pomodoro_app_model, now);
    if(!changed)
        return false;
    if(pomodoro_host->alarm_set(pomodoro_app_model.deadline_epoch,
                                pomodoro_app_model.alarm_token) < 0) {
        pomodoro_app_model = previous;
        pomodoro_alarm_error = true;
        return true;
    }
    pomodoro_alarm_error = false;
    pomodoro_last_render_remaining =
        pomodoro_remaining_at(&pomodoro_app_model, now);
    persist_model();
    return true;
}

static void enter_setup(void)
{
    pomodoro_setup_config = pomodoro_app_model.config;
    pomodoro_setup_index = 0;
    pomodoro_setup_editing = false;
    pomodoro_setup_active = true;
}

static bool commit_setup(void)
{
    if(!pomodoro_model_set_config(&pomodoro_app_model,
                                  &pomodoro_setup_config))
        return false;
    pomodoro_setup_active = false;
    pomodoro_setup_editing = false;
    pomodoro_action_index = 0;
    persist_model();
    if(CP_HOST_HAS(pomodoro_host, CP_CAP_UI_TOAST, ui_toast))
        (void)pomodoro_host->ui_toast(tr("Settings saved"), 1500);
    return true;
}

static bool activate_action(unsigned index)
{
    enum pomodoro_run_state state =
        (enum pomodoro_run_state)pomodoro_app_model.run_state;

    if(index >= timer_action_count())
        return false;
    if(index == 0) {
        if(state == POMODORO_READY)
            return start_or_resume();
        if(state == POMODORO_RUNNING) {
            pomodoro_pause(&pomodoro_app_model,
                           pomodoro_host->epoch_seconds());
            pomodoro_host->alarm_cancel();
            persist_model();
            return true;
        }
        if(state == POMODORO_PAUSED)
            return start_or_resume();
        acknowledge_alarm();
        pomodoro_host->alarm_cancel();
        if(pomodoro_advance(&pomodoro_app_model)) {
            pomodoro_action_index = 0;
            persist_model();
            return true;
        }
        return false;
    }

    if(state == POMODORO_READY) {
        enter_setup();
        return true;
    }
    if((state == POMODORO_RUNNING || state == POMODORO_PAUSED) &&
       index == 1) {
        pomodoro_host->alarm_cancel();
        acknowledge_alarm();
        if(pomodoro_skip(&pomodoro_app_model)) {
            pomodoro_action_index = 0;
            persist_model();
            return true;
        }
        return false;
    }

    pomodoro_host->alarm_cancel();
    acknowledge_alarm();
    if(pomodoro_reset(&pomodoro_app_model)) {
        pomodoro_action_index = 0;
        persist_model();
        return true;
    }
    return false;
}

static void move_action(int direction)
{
    unsigned count = timer_action_count();

    if(count == 0)
        return;
    if(direction > 0)
        pomodoro_action_index =
            (uint8_t)((pomodoro_action_index + 1u) % count);
    else
        pomodoro_action_index = (uint8_t)(
            (pomodoro_action_index + count - 1u) % count);
}

static uint16_t adjusted_value(uint16_t value, int direction,
                               unsigned steps, uint16_t minimum,
                               uint16_t maximum)
{
    unsigned amount = steps == 0 ? 1u : steps;

    if(direction > 0) {
        unsigned expanded = (unsigned)value + amount;
        return (uint16_t)(expanded > maximum ? maximum : expanded);
    }
    if(amount >= value || value - amount < minimum)
        return minimum;
    return (uint16_t)(value - amount);
}

static void adjust_setup(int direction, unsigned steps)
{
    if(pomodoro_setup_index == POMODORO_SETUP_FOCUS) {
        pomodoro_setup_config.focus_minutes = adjusted_value(
            pomodoro_setup_config.focus_minutes, direction, steps,
            POMODORO_MIN_MINUTES, POMODORO_MAX_MINUTES);
    }
    else if(pomodoro_setup_index == POMODORO_SETUP_SHORT) {
        pomodoro_setup_config.short_break_minutes = adjusted_value(
            pomodoro_setup_config.short_break_minutes, direction, steps,
            POMODORO_MIN_MINUTES, POMODORO_MAX_MINUTES);
    }
    else if(pomodoro_setup_index == POMODORO_SETUP_LONG) {
        pomodoro_setup_config.long_break_minutes = adjusted_value(
            pomodoro_setup_config.long_break_minutes, direction, steps,
            POMODORO_MIN_MINUTES, POMODORO_MAX_MINUTES);
    }
    else if(pomodoro_setup_index == POMODORO_SETUP_ROUNDS) {
        pomodoro_setup_config.rounds = adjusted_value(
            pomodoro_setup_config.rounds, direction, steps,
            POMODORO_MIN_ROUNDS, POMODORO_MAX_ROUNDS);
    }
}

static void move_setup(int direction)
{
    if(direction > 0)
        pomodoro_setup_index = (uint8_t)(
            (pomodoro_setup_index + 1) % POMODORO_SETUP_ITEM_COUNT);
    else
        pomodoro_setup_index = (uint8_t)(
            (pomodoro_setup_index + POMODORO_SETUP_ITEM_COUNT - 1) %
            POMODORO_SETUP_ITEM_COUNT);
}

static bool handle_setup_event(const struct cp_input_event *event)
{
    unsigned steps = event->steps == 0 ? 1u : event->steps;

    if(event->type == CP_INPUT_WHEEL_CLOCKWISE) {
        if(pomodoro_setup_editing)
            adjust_setup(1, steps);
        else
            move_setup(1);
        return true;
    }
    if(event->type == CP_INPUT_WHEEL_COUNTERCLOCKWISE) {
        if(pomodoro_setup_editing)
            adjust_setup(-1, steps);
        else
            move_setup(-1);
        return true;
    }
    if(event->type == CP_INPUT_LEFT &&
       pomodoro_setup_index != POMODORO_SETUP_DONE) {
        adjust_setup(-1, steps);
        return true;
    }
    if(event->type == CP_INPUT_RIGHT &&
       pomodoro_setup_index != POMODORO_SETUP_DONE) {
        adjust_setup(1, steps);
        return true;
    }
    if(event->type == CP_INPUT_SELECT && !event->repeated) {
        if(pomodoro_setup_index == POMODORO_SETUP_DONE)
            return commit_setup();
        pomodoro_setup_editing = !pomodoro_setup_editing;
        return true;
    }
    if(event->type == CP_INPUT_PLAY && !event->repeated)
        return commit_setup();
    if(event->type == CP_INPUT_MENU && !event->repeated) {
        if(pomodoro_setup_editing)
            pomodoro_setup_editing = false;
        else
            pomodoro_setup_active = false;
        return true;
    }
    return false;
}

static void app_open(void)
{
    struct pomodoro_disk_state disk;
    int read_size;
    uint32_t now;
    bool changed = false;

    pomodoro_model_init(&pomodoro_app_model);
    pomodoro_action_index = 0;
    pomodoro_setup_index = 0;
    pomodoro_setup_active = false;
    pomodoro_setup_editing = false;
    pomodoro_storage_error = false;
    pomodoro_alarm_error = false;

    read_size = pomodoro_host->state_read(&disk, sizeof(disk));
    if(read_size > 0 &&
       !pomodoro_unpack(&pomodoro_app_model, &disk, (size_t)read_size))
        pomodoro_storage_error = true;

    now = pomodoro_host->epoch_seconds();
    if(poll_alarm())
        changed = true;
    if(pomodoro_tick(&pomodoro_app_model, now)) {
        persist_model();
        changed = true;
    }
    if(pomodoro_app_model.run_state == POMODORO_RUNNING &&
       pomodoro_host->alarm_set(pomodoro_app_model.deadline_epoch,
                                pomodoro_app_model.alarm_token) < 0)
        pomodoro_alarm_error = true;
    if(changed)
        pomodoro_action_index = 0;
    pomodoro_last_render_remaining =
        pomodoro_remaining_at(&pomodoro_app_model, now);
}

static void app_close(void)
{
    persist_model();
}

static bool app_event(const struct cp_input_event *event)
{
    if(event == NULL || event->struct_size < sizeof(*event) ||
       event->type > CP_INPUT_MENU)
        return false;
    if(pomodoro_setup_active)
        return handle_setup_event(event);

    if(event->type == CP_INPUT_WHEEL_CLOCKWISE ||
       event->type == CP_INPUT_RIGHT) {
        move_action(1);
        return true;
    }
    if(event->type == CP_INPUT_WHEEL_COUNTERCLOCKWISE ||
       event->type == CP_INPUT_LEFT) {
        move_action(-1);
        return true;
    }
    if((event->type == CP_INPUT_SELECT ||
        event->type == CP_INPUT_PLAY) && !event->repeated) {
        if(event->type == CP_INPUT_PLAY)
            pomodoro_action_index = 0;
        return activate_action(pomodoro_action_index);
    }
    return false;
}

static bool app_tick(uint32_t epoch_seconds, uint32_t monotonic_ms)
{
    uint32_t remaining;
    bool changed = false;

    (void)monotonic_ms;
    if(poll_alarm())
        changed = true;
    if(pomodoro_tick(&pomodoro_app_model, epoch_seconds)) {
        persist_model();
        pomodoro_action_index = 0;
        changed = true;
    }
    remaining = pomodoro_remaining_at(&pomodoro_app_model, epoch_seconds);
    if(remaining != pomodoro_last_render_remaining) {
        pomodoro_last_render_remaining = remaining;
        changed = true;
    }
    return changed;
}

static void app_render(struct cp_scene *scene)
{
    cp_scene_reset(scene);
    if(pomodoro_setup_active)
        render_setup(scene);
    else
        render_timer(scene);
}

static const struct cp_miniapp_ops pomodoro_ops = {
    CP_MINIAPP_ABI_VERSION,
    sizeof(struct cp_miniapp_ops),
    "pomodoro",
    "Pomodoro",
    "1.2.0",
    app_open,
    app_close,
    app_event,
    app_tick,
    app_render
};

const struct cp_miniapp_ops *
cp_miniapp_entry(const struct cp_host_api *host)
{
    struct cp_system_info info;

    if(host == NULL ||
       host->abi_version != CP_MINIAPP_ABI_VERSION ||
       host->struct_size < sizeof(*host) ||
       host->epoch_seconds == NULL ||
       host->monotonic_ms == NULL ||
       host->state_read == NULL ||
       host->state_write == NULL ||
       host->alarm_set == NULL ||
       host->alarm_cancel == NULL ||
       host->alarm_fired == NULL ||
       host->alarm_acknowledge == NULL)
        return NULL;
    pomodoro_host = host;
    pomodoro_language = CP_LANGUAGE_ENGLISH;
    if(CP_HOST_HAS(host, CP_CAP_SYSTEM_INFO, system_info)) {
        info.struct_size = sizeof(info);
        if(host->system_info(&info) == 0 && info.language < CP_LANGUAGE_COUNT)
            pomodoro_language = info.language;
    }
    return &pomodoro_ops;
}

#ifdef CRAZYPOD_MINIAPP_PACKAGE
CP_MINIAPP_HEADER;
#endif
