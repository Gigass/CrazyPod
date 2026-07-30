#include "engine.h"
#include "../sdk/crazypod_miniapp.h"

#define CP_TR(text) (text)

#define GAME2048_HOME_MAX_ITEMS 5u
#define GAME2048_PAUSE_ITEMS 6u
#define GAME2048_SETTINGS_ITEMS 5u
#define GAME2048_HISTORY_ROWS 5u

enum game2048_view {
    GAME2048_VIEW_HOME = 0,
    GAME2048_VIEW_GAME,
    GAME2048_VIEW_PAUSE,
    GAME2048_VIEW_SETTINGS,
    GAME2048_VIEW_HISTORY,
    GAME2048_VIEW_HISTORY_DETAIL,
    GAME2048_VIEW_HELP,
    GAME2048_VIEW_WIN,
    GAME2048_VIEW_RESULT,
    GAME2048_VIEW_CONFIRM
};

enum game2048_home_action {
    GAME2048_HOME_CONTINUE = 0,
    GAME2048_HOME_NEW,
    GAME2048_HOME_HISTORY,
    GAME2048_HOME_SETTINGS,
    GAME2048_HOME_HELP
};

enum game2048_confirm_action {
    GAME2048_CONFIRM_NONE = 0,
    GAME2048_CONFIRM_NEW,
    GAME2048_CONFIRM_END,
    GAME2048_CONFIRM_CLEAR_HISTORY,
    GAME2048_CONFIRM_RESET_BEST
};

static const struct cp_host_api *game_host;
static struct game2048_model game_model;
static enum game2048_view game_view;
static enum game2048_view return_view;
static enum game2048_view confirm_return_view;
static enum game2048_confirm_action confirm_action;
static uint8_t focus_index;
static uint8_t detail_index;
static bool confirm_selected;
static bool storage_error;
static uint32_t last_render_elapsed;

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

static void format_unsigned(uint32_t value, char *buffer, size_t capacity)
{
    size_t length = 0;

    if(capacity == 0)
        return;
    buffer[0] = '\0';
    append_unsigned(buffer, capacity, &length, value);
}

static void format_duration(uint32_t seconds, char *buffer, size_t capacity)
{
    size_t length = 0;
    uint32_t minutes = seconds / 60u;
    uint32_t remainder = seconds % 60u;

    if(CP_HOST_HAS(game_host, CP_CAP_FORMAT_DURATION, format_duration)) {
        game_host->format_duration(seconds, buffer, capacity);
        return;
    }
    if(capacity == 0)
        return;
    buffer[0] = '\0';
    append_unsigned(buffer, capacity, &length, minutes);
    append_character(buffer, capacity, &length, ':');
    if(remainder < 10u)
        append_character(buffer, capacity, &length, '0');
    append_unsigned(buffer, capacity, &length, remainder);
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

static bool session_resumable(void)
{
    return game_model.session.phase == GAME2048_SESSION_ACTIVE ||
        game_model.session.phase == GAME2048_SESSION_WIN_PROMPT;
}

static bool persist_model(void)
{
    struct game2048_disk_state disk;
    int result;

    game2048_pack(&game_model, &disk, game_host->monotonic_ms());
    result = game_host->state_write(&disk, sizeof(disk));
    storage_error = result < 0;
    return !storage_error;
}

static void pause_game(void)
{
    game2048_pause_clock(
        &game_model, game_host->monotonic_ms());
}

static void enter_game(void)
{
    if(game_model.session.phase == GAME2048_SESSION_WIN_PROMPT) {
        game_view = GAME2048_VIEW_WIN;
        focus_index = 0;
        return;
    }
    if(game_model.session.phase == GAME2048_SESSION_FAILED) {
        game_view = GAME2048_VIEW_RESULT;
        focus_index = 0;
        return;
    }
    game2048_resume_clock(
        &game_model, game_host->monotonic_ms());
    last_render_elapsed = game2048_elapsed_seconds(
        &game_model, game_host->monotonic_ms());
    game_view = GAME2048_VIEW_GAME;
    focus_index = 0;
}

static bool start_new_game(void)
{
    if(!game2048_start_new(
           &game_model, game_host->epoch_seconds(),
           game_host->monotonic_ms()))
        return false;
    enter_game();
    persist_model();
    return true;
}

static void move_focus(unsigned count, int direction)
{
    if(count == 0)
        return;
    if(direction > 0)
        focus_index = (uint8_t)((focus_index + 1u) % count);
    else
        focus_index = (uint8_t)(
            (focus_index + count - 1u) % count);
}

static void draw_title(struct cp_scene *scene, const char *title,
                       const char *subtitle)
{
    add_text(scene, 14, 32, 180, 20, CP_FONT_TITLE,
             CP_ALIGN_LEFT, CP_COLOR_WHITE, title);
    add_text(scene, 194, 35, 112, 14, CP_FONT_CAPTION,
             CP_ALIGN_RIGHT, CP_COLOR_MUTED, subtitle);
}

static void draw_storage_error(struct cp_scene *scene)
{
    if(storage_error)
        add_text(scene, 14, 222, 292, 12, CP_FONT_CAPTION,
                 CP_ALIGN_CENTER, CP_COLOR_ERROR, "NOT SAVED");
}

static void draw_menu_row(struct cp_scene *scene, int y,
                          const char *label, const char *value,
                          bool focused)
{
    struct cp_draw_command *row = add_rect(
        scene, 18, y, 284, 28,
        focused ? CP_COLOR_ACCENT : CP_COLOR_SURFACE_RAISED, 8);

    if(row != NULL && focused) {
        row->flags = CP_DRAW_FOCUSED;
        row->border = CP_COLOR_ACCENT;
        row->border_width = 1;
        row->border_opacity = 255;
    }
    add_text(scene, 31, y + 5, value == NULL ? 258 : 176, 17,
             CP_FONT_BODY, CP_ALIGN_LEFT,
             focused ? CP_COLOR_ACCENT_FOREGROUND : CP_COLOR_WHITE,
             label);
    if(value != NULL)
        add_text(scene, 207, y + 5, 82, 17, CP_FONT_LABEL,
                 CP_ALIGN_RIGHT,
                 focused ? CP_COLOR_ACCENT_FOREGROUND :
                           CP_COLOR_MUTED,
                 value);
}

static unsigned home_item_count(void)
{
    return session_resumable()
        ? GAME2048_HOME_MAX_ITEMS
        : GAME2048_HOME_MAX_ITEMS - 1u;
}

static enum game2048_home_action home_action(unsigned index)
{
    if(session_resumable())
        return (enum game2048_home_action)index;
    return (enum game2048_home_action)(index + 1u);
}

static const char *home_label(enum game2048_home_action action)
{
    if(action == GAME2048_HOME_CONTINUE)
        return "CONTINUE";
    if(action == GAME2048_HOME_NEW)
        return "NEW GAME";
    if(action == GAME2048_HOME_HISTORY)
        return "HISTORY";
    if(action == GAME2048_HOME_SETTINGS)
        return "SETTINGS";
    return "HOW TO PLAY";
}

static void render_home(struct cp_scene *scene)
{
    char best[CP_MINIAPP_TEXT_SIZE];
    char subtitle[CP_MINIAPP_TEXT_SIZE];
    size_t length = 0;
    unsigned count = home_item_count();
    unsigned index;

    format_unsigned(game_model.high_score, best, sizeof(best));
    subtitle[0] = '\0';
    append_text(subtitle, sizeof(subtitle), &length, "BEST ");
    append_text(subtitle, sizeof(subtitle), &length, best);
    draw_title(scene, "2048", subtitle);
    add_text(scene, 18, 52, 284, 13, CP_FONT_CAPTION,
             CP_ALIGN_LEFT, CP_COLOR_MUTED,
             "CLICK WHEEL EDITION");
    for(index = 0; index < count; ++index) {
        enum game2048_home_action action = home_action(index);
        char history_count[8];
        const char *value = NULL;

        if(action == GAME2048_HOME_HISTORY) {
            format_unsigned(game_model.history_count,
                            history_count, sizeof(history_count));
            value = history_count;
        }
        draw_menu_row(scene, 68 + (int)index * 30,
                      home_label(action), value,
                      index == focus_index);
    }
    add_text(scene, 18, 222, 284, 12, CP_FONT_CAPTION,
             CP_ALIGN_CENTER, CP_COLOR_MUTED,
             "WHEEL SELECT  CENTER OPEN");
    draw_storage_error(scene);
}

static enum cp_color_token tile_color(uint8_t exponent)
{
    if(exponent == 0)
        return CP_COLOR_SURFACE_RAISED;
    if(exponent <= 2)
        return CP_COLOR_SURFACE;
    if(exponent == 3)
        return CP_COLOR_AMBER;
    if(exponent == 4)
        return CP_COLOR_ROSE;
    if(exponent == 5)
        return CP_COLOR_ERROR;
    if(exponent == 6)
        return CP_COLOR_CYAN;
    if(exponent <= 9)
        return CP_COLOR_GREEN;
    return CP_COLOR_ACCENT;
}

static enum cp_color_token tile_foreground(uint8_t exponent)
{
    return exponent >= 10
        ? CP_COLOR_ACCENT_FOREGROUND : CP_COLOR_WHITE;
}

static void draw_board(struct cp_scene *scene, const uint8_t *board,
                       int x, int y, int cell, int gap, bool compact)
{
    unsigned index;

    for(index = 0; index < GAME2048_BOARD_CELLS; ++index) {
        unsigned row = index / 4u;
        unsigned column = index % 4u;
        int tile_x = x + (int)column * (cell + gap);
        int tile_y = y + (int)row * (cell + gap);
        uint8_t exponent = board[index];
        char value[12] = "";

        add_rect(scene, tile_x, tile_y, cell, cell,
                 tile_color(exponent), compact ? 6 : 9);
        if(exponent == 0)
            continue;
        format_unsigned(game2048_tile_value(exponent),
                        value, sizeof(value));
        add_text(scene, tile_x + 2, tile_y, cell - 4, cell,
                 compact ? CP_FONT_CAPTION :
                 exponent <= 6 ? CP_FONT_NUMBER : CP_FONT_BODY,
                 CP_ALIGN_CENTER, tile_foreground(exponent), value);
    }
}

static void render_game(struct cp_scene *scene)
{
    char score[16];
    char best[16];
    char elapsed[16];
    char moves[16];
    char center_stat[CP_MINIAPP_TEXT_SIZE];
    char text[CP_MINIAPP_TEXT_SIZE];
    size_t length = 0;

    format_unsigned(game_model.session.score, score, sizeof(score));
    format_unsigned(game_model.high_score, best, sizeof(best));
    text[0] = '\0';
    append_text(text, sizeof(text), &length, "SCORE ");
    append_text(text, sizeof(text), &length, score);
    add_text(scene, 8, 32, 108, 18, CP_FONT_LABEL,
             CP_ALIGN_LEFT, CP_COLOR_WHITE, text);
    text[0] = '\0';
    length = 0;
    append_text(text, sizeof(text), &length, "BEST ");
    append_text(text, sizeof(text), &length, best);
    add_text(scene, 212, 32, 100, 18, CP_FONT_LABEL,
             CP_ALIGN_RIGHT, CP_COLOR_MUTED, text);
    format_unsigned(game_model.session.moves, moves, sizeof(moves));
    center_stat[0] = '\0';
    length = 0;
    if(game_model.settings.show_timer) {
        format_duration(game2048_elapsed_seconds(
            &game_model, game_host->monotonic_ms()),
            elapsed, sizeof(elapsed));
        append_text(center_stat, sizeof(center_stat), &length, elapsed);
        append_text(center_stat, sizeof(center_stat), &length, " / ");
        append_text(center_stat, sizeof(center_stat), &length, moves);
    } else {
        append_text(center_stat, sizeof(center_stat), &length, "MOVES ");
        append_text(center_stat, sizeof(center_stat), &length, moves);
    }
    add_text(scene, 116, 32, 92, 18, CP_FONT_CAPTION,
             CP_ALIGN_CENTER, CP_COLOR_MUTED, center_stat);
    add_rect(scene, 62, 51, 196, 183,
             CP_COLOR_SURFACE, 12);
    draw_board(scene, game_model.session.board,
               70, 59, 42, 3, false);
    draw_storage_error(scene);
}

static const char *pause_label(unsigned index)
{
    static const char *const labels[GAME2048_PAUSE_ITEMS] = {
        "CONTINUE", "RESTART", "END GAME",
        "HISTORY", "SETTINGS", "HOME"
    };

    return index < GAME2048_PAUSE_ITEMS ? labels[index] : "";
}

static void render_pause(struct cp_scene *scene)
{
    unsigned index;

    draw_title(scene, CP_TR("PAUSED"), CP_TR("GAME SAVED"));
    for(index = 0; index < GAME2048_PAUSE_ITEMS; ++index)
        draw_menu_row(scene, 51 + (int)index * 29,
                      pause_label(index), NULL,
                      index == focus_index);
    draw_storage_error(scene);
}

static const char *setting_label(unsigned index)
{
    static const char *const labels[GAME2048_SETTINGS_ITEMS] = {
        "AUTO CONTINUE AT 2048", "SHOW TIMER",
        "CLEAR HISTORY", "RESET BEST", "BACK"
    };

    return index < GAME2048_SETTINGS_ITEMS ? labels[index] : "";
}

static const char *setting_value(unsigned index)
{
    if(index == 0)
        return game_model.settings.auto_continue ? "ON" : "OFF";
    if(index == 1)
        return game_model.settings.show_timer ? "ON" : "OFF";
    return NULL;
}

static void render_settings(struct cp_scene *scene)
{
    unsigned index;

    draw_title(scene, CP_TR("SETTINGS"), CP_TR("CENTER CHANGE"));
    for(index = 0; index < GAME2048_SETTINGS_ITEMS; ++index)
        draw_menu_row(scene, 60 + (int)index * 33,
                      setting_label(index), setting_value(index),
                      index == focus_index);
    draw_storage_error(scene);
}

static const char *result_name(uint8_t result)
{
    if(result == GAME2048_RESULT_WIN)
        return "WIN";
    if(result == GAME2048_RESULT_POST_WIN_FAILED)
        return "2048 + FAILED";
    if(result == GAME2048_RESULT_ABANDONED)
        return "ABANDONED";
    return "FAILED";
}

static void format_record_summary(const struct game2048_record *record,
                                  char *buffer, size_t capacity)
{
    size_t length = 0;
    char value[16];

    buffer[0] = '\0';
    append_text(buffer, capacity, &length, "SCORE ");
    format_unsigned(record->score, value, sizeof(value));
    append_text(buffer, capacity, &length, value);
    append_text(buffer, capacity, &length, "  TILE ");
    format_unsigned(game2048_tile_value(
        game2048_max_exponent(record->board)),
        value, sizeof(value));
    append_text(buffer, capacity, &length, value);
}

static void render_history(struct cp_scene *scene)
{
    unsigned count = game_model.history_count;
    unsigned start = 0;
    unsigned row;

    draw_title(scene, CP_TR("HISTORY"), CP_TR("RECENT 10"));
    if(count == 0) {
        add_text(scene, 30, 112, 260, 20, CP_FONT_BODY,
                 CP_ALIGN_CENTER, CP_COLOR_MUTED,
                 "NO FINISHED GAMES");
        add_text(scene, 30, 144, 260, 14, CP_FONT_CAPTION,
                 CP_ALIGN_CENTER, CP_COLOR_MUTED,
                 "MENU BACK");
        return;
    }
    if(focus_index >= GAME2048_HISTORY_ROWS)
        start = focus_index - GAME2048_HISTORY_ROWS + 1u;
    for(row = 0; row < GAME2048_HISTORY_ROWS; ++row) {
        unsigned index = start + row;
        const struct game2048_record *record;
        char summary[CP_MINIAPP_TEXT_SIZE];
        int y;
        bool focused;
        struct cp_draw_command *box;

        if(index >= count)
            break;
        record = game2048_history_get(&game_model, index);
        y = 57 + (int)row * 33;
        focused = index == focus_index;
        box = add_rect(scene, 18, y, 284, 30,
                       focused ? CP_COLOR_ACCENT :
                                 CP_COLOR_SURFACE_RAISED, 8);
        if(box != NULL && focused)
            box->flags = CP_DRAW_FOCUSED;
        add_text(scene, 29, y + 3, 112, 14, CP_FONT_LABEL,
                 CP_ALIGN_LEFT,
                 focused ? CP_COLOR_ACCENT_FOREGROUND :
                           CP_COLOR_WHITE,
                 result_name(record->result));
        format_record_summary(record, summary, sizeof(summary));
        add_text(scene, 29, y + 17, 260, 12, CP_FONT_CAPTION,
                 CP_ALIGN_LEFT,
                 focused ? CP_COLOR_ACCENT_FOREGROUND :
                           CP_COLOR_MUTED,
                 summary);
    }
    add_text(scene, 18, 220, 284, 12, CP_FONT_CAPTION,
             CP_ALIGN_CENTER, CP_COLOR_MUTED,
             "CENTER DETAILS  MENU BACK");
}

static void render_history_detail(struct cp_scene *scene)
{
    const struct game2048_record *record =
        game2048_history_get(&game_model, detail_index);
    char text[CP_MINIAPP_TEXT_SIZE];
    char value[16];
    size_t length;

    if(record == NULL) {
        game_view = GAME2048_VIEW_HISTORY;
        render_history(scene);
        return;
    }
    draw_title(scene, result_name(record->result), CP_TR("FINAL BOARD"));
    add_rect(scene, 15, 57, 140, 140, CP_COLOR_SURFACE, 10);
    draw_board(scene, record->board, 21, 63, 29, 3, true);

    format_record_summary(record, text, sizeof(text));
    add_text(scene, 168, 61, 138, 30, CP_FONT_BODY,
             CP_ALIGN_LEFT, CP_COLOR_WHITE, text);
    length = 0;
    text[0] = '\0';
    append_text(text, sizeof(text), &length, "MOVES ");
    format_unsigned(record->moves, value, sizeof(value));
    append_text(text, sizeof(text), &length, value);
    add_text(scene, 168, 99, 138, 16, CP_FONT_LABEL,
             CP_ALIGN_LEFT, CP_COLOR_MUTED, text);
    length = 0;
    text[0] = '\0';
    append_text(text, sizeof(text), &length, "TIME ");
    format_duration(record->elapsed_seconds,
                    value, sizeof(value));
    append_text(text, sizeof(text), &length, value);
    add_text(scene, 168, 121, 138, 16, CP_FONT_LABEL,
             CP_ALIGN_LEFT, CP_COLOR_MUTED, text);
    if(CP_HOST_HAS(game_host, CP_CAP_FORMAT_DATETIME,
                   format_datetime)) {
        game_host->format_datetime(
            record->ended_epoch, CP_DATETIME_DATE,
            text, sizeof(text));
        add_text(scene, 168, 143, 138, 16, CP_FONT_CAPTION,
                 CP_ALIGN_LEFT, CP_COLOR_MUTED, text);
    }
    add_text(scene, 168, 170, 138, 18, CP_FONT_BODY,
             CP_ALIGN_LEFT, record->reached_2048
                 ? CP_COLOR_GREEN : CP_COLOR_MUTED,
             record->reached_2048 ? "2048 REACHED" : "UNDER 2048");
    add_text(scene, 18, 216, 284, 14, CP_FONT_CAPTION,
             CP_ALIGN_CENTER, CP_COLOR_MUTED,
             "CENTER OR MENU BACK");
}

static void render_help(struct cp_scene *scene)
{
    draw_title(scene, CP_TR("HOW TO PLAY"), CP_TR("JOIN THE TILES"));
    add_text(scene, 24, 48, 272, 22, CP_FONT_BODY,
             CP_ALIGN_CENTER, CP_COLOR_WHITE,
             "MENU  UP");
    add_text(scene, 24, 76, 272, 22, CP_FONT_BODY,
             CP_ALIGN_CENTER, CP_COLOR_WHITE,
             "PLAY  DOWN");
    add_text(scene, 24, 104, 272, 22, CP_FONT_BODY,
             CP_ALIGN_CENTER, CP_COLOR_WHITE,
             "LEFT / RIGHT  MOVE");
    add_text(scene, 24, 140, 272, 34, CP_FONT_BODY,
             CP_ALIGN_CENTER, CP_COLOR_MUTED,
             "MATCH EQUAL TILES TO MERGE");
    add_text(scene, 24, 180, 272, 20, CP_FONT_BODY,
             CP_ALIGN_CENTER, CP_COLOR_AMBER,
             "REACH 2048");
    add_text(scene, 24, 216, 272, 14, CP_FONT_CAPTION,
             CP_ALIGN_CENTER, CP_COLOR_MUTED,
             "CENTER OR MENU BACK");
}

static const char *confirm_title(void)
{
    if(confirm_action == GAME2048_CONFIRM_NEW)
        return "START NEW GAME?";
    if(confirm_action == GAME2048_CONFIRM_END)
        return "END THIS GAME?";
    if(confirm_action == GAME2048_CONFIRM_CLEAR_HISTORY)
        return "CLEAR HISTORY?";
    return "RESET BEST SCORE?";
}

static void render_confirm(struct cp_scene *scene)
{
    draw_title(scene, CP_TR("CONFIRM"), CP_TR("NO UNDO"));
    add_rect(scene, 35, 68, 250, 116,
             CP_COLOR_SURFACE, 12);
    add_text(scene, 50, 84, 220, 25, CP_FONT_TITLE,
             CP_ALIGN_CENTER, CP_COLOR_WHITE,
             confirm_title());
    draw_menu_row(scene, 120, "CANCEL", NULL,
                  !confirm_selected);
    draw_menu_row(scene, 154, "CONFIRM", NULL,
                  confirm_selected);
}

static void render_win(struct cp_scene *scene)
{
    char score[16];

    format_unsigned(game_model.session.score, score, sizeof(score));
    draw_title(scene, CP_TR("2048 REACHED"), CP_TR("YOU WIN"));
    add_text(scene, 30, 55, 260, 48, CP_FONT_DISPLAY,
             CP_ALIGN_CENTER, CP_COLOR_AMBER, "2048");
    add_text(scene, 30, 105, 260, 18, CP_FONT_LABEL,
             CP_ALIGN_CENTER, CP_COLOR_MUTED, score);
    draw_menu_row(scene, 137, "CONTINUE", NULL,
                  focus_index == 0);
    draw_menu_row(scene, 172, "FINISH HERE", NULL,
                  focus_index == 1);
}

static void render_result(struct cp_scene *scene)
{
    const struct game2048_record *record =
        game2048_history_get(&game_model, 0);
    char score[16] = "0";
    const char *title = "GAME OVER";
    unsigned index;
    static const char *const actions[] = {
        "NEW GAME", "HISTORY", "HOME"
    };

    if(record != NULL) {
        format_unsigned(record->score, score, sizeof(score));
        if(record->result == GAME2048_RESULT_WIN)
            title = "2048 COMPLETE";
        else if(record->result == GAME2048_RESULT_POST_WIN_FAILED)
            title = "RUN COMPLETE";
    }
    draw_title(scene, title, record != NULL
               ? result_name(record->result) : "");
    add_text(scene, 30, 47, 260, 45, CP_FONT_DISPLAY,
             CP_ALIGN_CENTER,
             record != NULL &&
             record->result == GAME2048_RESULT_WIN
                 ? CP_COLOR_GREEN : CP_COLOR_WHITE,
             score);
    add_text(scene, 30, 92, 260, 14, CP_FONT_CAPTION,
             CP_ALIGN_CENTER, CP_COLOR_MUTED, "FINAL SCORE");
    for(index = 0; index < 3; ++index)
        draw_menu_row(scene, 120 + (int)index * 34,
                      actions[index], NULL,
                      index == focus_index);
}

static void app_render(struct cp_scene *scene)
{
    if(scene == NULL)
        return;
    cp_scene_reset(scene);
    scene->background = CP_COLOR_BACKGROUND;
    if(game_view == GAME2048_VIEW_HOME)
        render_home(scene);
    else if(game_view == GAME2048_VIEW_GAME)
        render_game(scene);
    else if(game_view == GAME2048_VIEW_PAUSE)
        render_pause(scene);
    else if(game_view == GAME2048_VIEW_SETTINGS)
        render_settings(scene);
    else if(game_view == GAME2048_VIEW_HISTORY)
        render_history(scene);
    else if(game_view == GAME2048_VIEW_HISTORY_DETAIL)
        render_history_detail(scene);
    else if(game_view == GAME2048_VIEW_HELP)
        render_help(scene);
    else if(game_view == GAME2048_VIEW_WIN)
        render_win(scene);
    else if(game_view == GAME2048_VIEW_RESULT)
        render_result(scene);
    else
        render_confirm(scene);
}

static void show_confirm(enum game2048_confirm_action action,
                         enum game2048_view cancel_view)
{
    pause_game();
    confirm_action = action;
    confirm_return_view = cancel_view;
    confirm_selected = false;
    game_view = GAME2048_VIEW_CONFIRM;
}

static bool activate_home(void)
{
    enum game2048_home_action action = home_action(focus_index);

    if(action == GAME2048_HOME_CONTINUE) {
        enter_game();
        return true;
    }
    if(action == GAME2048_HOME_NEW) {
        if(session_resumable())
            show_confirm(GAME2048_CONFIRM_NEW,
                         GAME2048_VIEW_HOME);
        else
            start_new_game();
        return true;
    }
    if(action == GAME2048_HOME_HISTORY) {
        return_view = GAME2048_VIEW_HOME;
        game_view = GAME2048_VIEW_HISTORY;
        focus_index = 0;
        return true;
    }
    if(action == GAME2048_HOME_SETTINGS) {
        return_view = GAME2048_VIEW_HOME;
        game_view = GAME2048_VIEW_SETTINGS;
        focus_index = 0;
        return true;
    }
    return_view = GAME2048_VIEW_HOME;
    game_view = GAME2048_VIEW_HELP;
    return true;
}

static bool activate_pause(void)
{
    if(focus_index == 0) {
        enter_game();
        return true;
    }
    if(focus_index == 1) {
        show_confirm(GAME2048_CONFIRM_NEW,
                     GAME2048_VIEW_PAUSE);
        return true;
    }
    if(focus_index == 2) {
        show_confirm(GAME2048_CONFIRM_END,
                     GAME2048_VIEW_PAUSE);
        return true;
    }
    if(focus_index == 3) {
        return_view = GAME2048_VIEW_PAUSE;
        game_view = GAME2048_VIEW_HISTORY;
        focus_index = 0;
        return true;
    }
    if(focus_index == 4) {
        return_view = GAME2048_VIEW_PAUSE;
        game_view = GAME2048_VIEW_SETTINGS;
        focus_index = 0;
        return true;
    }
    persist_model();
    game_view = GAME2048_VIEW_HOME;
    focus_index = 0;
    return true;
}

static bool activate_settings(void)
{
    if(focus_index == 0)
        game_model.settings.auto_continue =
            !game_model.settings.auto_continue;
    else if(focus_index == 1)
        game_model.settings.show_timer =
            !game_model.settings.show_timer;
    else if(focus_index == 2) {
        show_confirm(GAME2048_CONFIRM_CLEAR_HISTORY,
                     GAME2048_VIEW_SETTINGS);
        return true;
    }
    else if(focus_index == 3) {
        show_confirm(GAME2048_CONFIRM_RESET_BEST,
                     GAME2048_VIEW_SETTINGS);
        return true;
    }
    else {
        game_view = return_view;
        focus_index = 0;
        return true;
    }
    persist_model();
    return true;
}

static bool execute_confirm(void)
{
    enum game2048_confirm_action action = confirm_action;

    confirm_action = GAME2048_CONFIRM_NONE;
    if(action == GAME2048_CONFIRM_NEW) {
        if(session_resumable())
            game2048_abandon(
                &game_model, game_host->epoch_seconds(),
                game_host->monotonic_ms());
        return start_new_game();
    }
    if(action == GAME2048_CONFIRM_END) {
        game2048_abandon(
            &game_model, game_host->epoch_seconds(),
            game_host->monotonic_ms());
        persist_model();
        game_view = GAME2048_VIEW_RESULT;
        focus_index = 0;
        return true;
    }
    if(action == GAME2048_CONFIRM_CLEAR_HISTORY)
        game2048_clear_history(&game_model);
    else if(action == GAME2048_CONFIRM_RESET_BEST)
        game2048_reset_high_score(&game_model);
    persist_model();
    game_view = GAME2048_VIEW_SETTINGS;
    focus_index = 0;
    return true;
}

static bool handle_game_event(const struct cp_input_event *event)
{
    enum game2048_direction direction;
    bool moved;

    if(event->repeated)
        return true;
    if(event->type == CP_INPUT_SELECT) {
        pause_game();
        persist_model();
        game_view = GAME2048_VIEW_PAUSE;
        focus_index = 0;
        return true;
    }
    if(event->type == CP_INPUT_MENU)
        direction = GAME2048_UP;
    else if(event->type == CP_INPUT_PLAY)
        direction = GAME2048_DOWN;
    else if(event->type == CP_INPUT_LEFT)
        direction = GAME2048_LEFT;
    else if(event->type == CP_INPUT_RIGHT)
        direction = GAME2048_RIGHT;
    else
        return true;
    moved = game2048_move(
        &game_model, direction, game_host->epoch_seconds(),
        game_host->monotonic_ms());
    if(!moved)
        return true;
    persist_model();
    if(game_model.session.phase == GAME2048_SESSION_WIN_PROMPT) {
        game_view = GAME2048_VIEW_WIN;
        focus_index = 0;
    }
    else if(game_model.session.phase == GAME2048_SESSION_FAILED) {
        game_view = GAME2048_VIEW_RESULT;
        focus_index = 0;
    }
    return true;
}

static bool handle_list_navigation(const struct cp_input_event *event,
                                   unsigned count)
{
    if(event->type == CP_INPUT_WHEEL_CLOCKWISE) {
        move_focus(count, 1);
        return true;
    }
    if(event->type == CP_INPUT_WHEEL_COUNTERCLOCKWISE) {
        move_focus(count, -1);
        return true;
    }
    return false;
}

static bool app_event(const struct cp_input_event *event)
{
    if(event == NULL || event->struct_size < sizeof(*event) ||
       event->type > CP_INPUT_MENU)
        return false;
    if(game_view == GAME2048_VIEW_GAME)
        return handle_game_event(event);

    if(game_view == GAME2048_VIEW_HOME) {
        if(handle_list_navigation(event, home_item_count()))
            return true;
        if(event->type == CP_INPUT_SELECT && !event->repeated)
            return activate_home();
        return true;
    }
    if(game_view == GAME2048_VIEW_PAUSE) {
        if(handle_list_navigation(event, GAME2048_PAUSE_ITEMS))
            return true;
        if(event->type == CP_INPUT_SELECT && !event->repeated)
            return activate_pause();
        if(event->type == CP_INPUT_MENU && !event->repeated) {
            enter_game();
            return true;
        }
        return true;
    }
    if(game_view == GAME2048_VIEW_SETTINGS) {
        if(handle_list_navigation(event, GAME2048_SETTINGS_ITEMS))
            return true;
        if(event->type == CP_INPUT_SELECT && !event->repeated)
            return activate_settings();
        if((event->type == CP_INPUT_MENU ||
            event->type == CP_INPUT_LEFT) && !event->repeated) {
            game_view = return_view;
            focus_index = 0;
            return true;
        }
        return true;
    }
    if(game_view == GAME2048_VIEW_HISTORY) {
        if(handle_list_navigation(
               event, game_model.history_count))
            return true;
        if(event->type == CP_INPUT_SELECT && !event->repeated &&
           game_model.history_count > 0) {
            detail_index = focus_index;
            game_view = GAME2048_VIEW_HISTORY_DETAIL;
            return true;
        }
        if((event->type == CP_INPUT_MENU ||
            event->type == CP_INPUT_LEFT) && !event->repeated) {
            game_view = return_view;
            focus_index = 0;
            return true;
        }
        return true;
    }
    if(game_view == GAME2048_VIEW_HISTORY_DETAIL ||
       game_view == GAME2048_VIEW_HELP) {
        if((event->type == CP_INPUT_SELECT ||
            event->type == CP_INPUT_MENU ||
            event->type == CP_INPUT_LEFT) && !event->repeated) {
            game_view = game_view == GAME2048_VIEW_HELP
                ? return_view : GAME2048_VIEW_HISTORY;
            return true;
        }
        return true;
    }
    if(game_view == GAME2048_VIEW_CONFIRM) {
        if(event->type == CP_INPUT_WHEEL_CLOCKWISE ||
           event->type == CP_INPUT_RIGHT)
            confirm_selected = true;
        else if(event->type == CP_INPUT_WHEEL_COUNTERCLOCKWISE ||
                event->type == CP_INPUT_LEFT)
            confirm_selected = false;
        else if(event->type == CP_INPUT_SELECT &&
                !event->repeated) {
            if(confirm_selected)
                return execute_confirm();
            game_view = confirm_return_view;
        }
        else if(event->type == CP_INPUT_MENU &&
                !event->repeated)
            game_view = confirm_return_view;
        return true;
    }
    if(game_view == GAME2048_VIEW_WIN) {
        if(handle_list_navigation(event, 2))
            return true;
        if(event->type == CP_INPUT_SELECT && !event->repeated) {
            if(focus_index == 0) {
                game2048_continue_after_win(
                    &game_model, game_host->epoch_seconds(),
                    game_host->monotonic_ms());
                persist_model();
                enter_game();
            }
            else {
                game2048_finish_win(
                    &game_model, game_host->epoch_seconds(),
                    game_host->monotonic_ms());
                persist_model();
                game_view = GAME2048_VIEW_RESULT;
                focus_index = 0;
            }
        }
        return true;
    }
    if(game_view == GAME2048_VIEW_RESULT) {
        if(handle_list_navigation(event, 3))
            return true;
        if(event->type == CP_INPUT_SELECT && !event->repeated) {
            if(focus_index == 0)
                start_new_game();
            else if(focus_index == 1) {
                return_view = GAME2048_VIEW_RESULT;
                game_view = GAME2048_VIEW_HISTORY;
                focus_index = 0;
            }
            else {
                game_view = GAME2048_VIEW_HOME;
                focus_index = 0;
            }
        }
        else if(event->type == CP_INPUT_MENU &&
                !event->repeated) {
            game_view = GAME2048_VIEW_HOME;
            focus_index = 0;
        }
        return true;
    }
    return true;
}

static bool app_tick(uint32_t epoch_seconds, uint32_t monotonic_ms)
{
    uint32_t elapsed;

    (void)epoch_seconds;
    if(game_view != GAME2048_VIEW_GAME ||
       !game_model.settings.show_timer)
        return false;
    elapsed = game2048_elapsed_seconds(&game_model, monotonic_ms);
    if(elapsed == last_render_elapsed)
        return false;
    last_render_elapsed = elapsed;
    return true;
}

static void app_open(void)
{
    struct game2048_disk_state disk;
    int read_size;

    game2048_model_init(&game_model);
    storage_error = false;
    read_size = game_host->state_read(&disk, sizeof(disk));
    if(read_size > 0 &&
       !game2048_unpack(&game_model, &disk, (size_t)read_size))
        storage_error = true;
    game_view = GAME2048_VIEW_HOME;
    return_view = GAME2048_VIEW_HOME;
    confirm_return_view = GAME2048_VIEW_HOME;
    confirm_action = GAME2048_CONFIRM_NONE;
    focus_index = 0;
    detail_index = 0;
    confirm_selected = false;
    last_render_elapsed = 0;
}

static void app_close(void)
{
    pause_game();
    persist_model();
}

static const struct cp_miniapp_ops game_ops = {
    CP_MINIAPP_ABI_VERSION,
    sizeof(struct cp_miniapp_ops),
    "game2048",
    "2048",
    "1.0.0",
    app_open,
    app_close,
    app_event,
    app_tick,
    app_render
};

const struct cp_miniapp_ops *
cp_miniapp_entry(const struct cp_host_api *host)
{
    if(host == NULL ||
       host->abi_version != CP_MINIAPP_ABI_VERSION ||
       host->struct_size < CP_HOST_API_V1_SIZE ||
       host->epoch_seconds == NULL ||
       host->monotonic_ms == NULL ||
       host->state_read == NULL ||
       host->state_write == NULL)
        return NULL;
    game_host = host;
    return &game_ops;
}

#ifdef CRAZYPOD_MINIAPP_PACKAGE
CP_MINIAPP_HEADER;
#endif
