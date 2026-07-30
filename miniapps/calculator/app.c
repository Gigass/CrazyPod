#include "engine.h"
#include "../sdk/crazypod_miniapp.h"
#include "../sdk/crazypod_miniapp_l10n.h"

#define CALCULATOR_PANEL_X 10
#define CALCULATOR_PANEL_Y 40
#define CALCULATOR_PANEL_WIDTH 300
#define CALCULATOR_PANEL_HEIGHT 188

#define CALCULATOR_KEYBOARD_X 16
#define CALCULATOR_KEYBOARD_Y 116
#define CALCULATOR_KEY_WIDTH 69
#define CALCULATOR_KEY_HEIGHT 20
#define CALCULATOR_COLUMN_GAP 4
#define CALCULATOR_ROW_GAP 2

static const struct cp_host_api *calculator_host;
static struct cp_calculator calculator;
static int calculator_focus;
static uint32_t calculator_language;

static size_t local_text_length(const char *text)
{
    size_t length = 0;

    if(text == NULL)
        return 0;
    while(text[length] != '\0')
        ++length;
    return length;
}

static void text_append(char *destination, size_t capacity,
                        const char *source)
{
    size_t output = local_text_length(destination);
    size_t input = 0;

    if(destination == NULL || source == NULL || output >= capacity)
        return;
    while(output + 1 < capacity && source[input] != '\0')
        destination[output++] = source[input++];
    destination[output] = '\0';
}

static void format_value(double value, char *buffer, size_t capacity)
{
    if(buffer == NULL || capacity == 0)
        return;
    if(calculator_host != NULL &&
       calculator_host->format_number != NULL) {
        calculator_host->format_number(value, buffer, capacity);
        buffer[capacity - 1] = '\0';
    } else {
        cp_text_copy(buffer, capacity, "0");
    }
}

static void format_current_value(char *buffer, size_t capacity)
{
    if(calculator.error != CP_CALCULATOR_ERROR_NONE) {
        cp_text_copy(buffer, capacity,
                     cp_l10n_text(calculator_language, "Error"));
    } else if(calculator.entering) {
        cp_text_copy(buffer, capacity,
                     cp_calculator_entry_text(&calculator));
    } else {
        format_value(calculator.display_value, buffer, capacity);
    }
}

static void format_expression(char *buffer, size_t capacity)
{
    char number[CP_MINIAPP_TEXT_SIZE];

    cp_text_copy(buffer, capacity, "");
    if(calculator.error == CP_CALCULATOR_ERROR_DIVIDE_BY_ZERO) {
        cp_text_copy(buffer, capacity,
                     cp_l10n_text(calculator_language,
                                  "Cannot divide by zero"));
        return;
    }
    if(calculator.error == CP_CALCULATOR_ERROR_RANGE) {
        cp_text_copy(buffer, capacity,
                     cp_l10n_text(calculator_language,
                                  "Result out of range"));
        return;
    }

    if(calculator.has_pending_operator) {
        format_value(calculator.accumulator, number, sizeof(number));
        text_append(buffer, capacity, number);
        text_append(buffer, capacity, " ");
        text_append(buffer, capacity,
                    cp_calculator_operator_symbol(
                        calculator.pending_operator));
        if(calculator.right_operand_ready) {
            text_append(buffer, capacity, " ");
            if(calculator.entering)
                cp_text_copy(number, sizeof(number),
                             cp_calculator_entry_text(&calculator));
            else
                format_value(calculator.display_value, number,
                             sizeof(number));
            text_append(buffer, capacity, number);
        }
    } else if(calculator.just_evaluated &&
              calculator.repeat_operator !=
                  CP_CALCULATOR_OPERATOR_NONE) {
        format_value(calculator.last_left_operand, number,
                     sizeof(number));
        text_append(buffer, capacity, number);
        text_append(buffer, capacity, " ");
        text_append(buffer, capacity,
                    cp_calculator_operator_symbol(
                        calculator.repeat_operator));
        text_append(buffer, capacity, " ");
        format_value(calculator.repeat_operand, number,
                     sizeof(number));
        text_append(buffer, capacity, number);
        text_append(buffer, capacity, " =");
    }
}

static struct cp_draw_command *add_rectangle(
    struct cp_scene *scene, int x, int y, int width, int height,
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
    command->border_opacity = 0;
    return command;
}

static struct cp_draw_command *add_text(
    struct cp_scene *scene, int x, int y, int width, int height,
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

static const char *key_label(int index)
{
    static const char *const labels[CP_CALCULATOR_KEY_COUNT] = {
        "",
        "\xc2\xb1",
        "%",
        "\xc3\xb7",
        "7", "8", "9",
        "\xc3\x97",
        "4", "5", "6",
        "\xe2\x88\x92",
        "1", "2", "3",
        "+",
        "0", ".",
        "\xe2\x8c\xab",
        "="
    };

    if(index == CP_CALCULATOR_KEY_CLEAR)
        return cp_calculator_clear_is_all(&calculator) ? "AC" : "C";
    if(index < 0 || index >= CP_CALCULATOR_KEY_COUNT)
        return "";
    return labels[index];
}

static enum cp_color_token key_foreground(int index)
{
    if(index == CP_CALCULATOR_KEY_CLEAR)
        return CP_COLOR_ROSE;
    if(index == CP_CALCULATOR_KEY_EQUALS)
        return CP_COLOR_GREEN;
    if(index == CP_CALCULATOR_KEY_SIGN ||
       index == CP_CALCULATOR_KEY_PERCENT ||
       index == CP_CALCULATOR_KEY_DIVIDE ||
       index == CP_CALCULATOR_KEY_MULTIPLY ||
       index == CP_CALCULATOR_KEY_SUBTRACT ||
       index == CP_CALCULATOR_KEY_ADD)
        return CP_COLOR_AMBER;
    return CP_COLOR_WHITE;
}

static void calculator_open(void)
{
    cp_calculator_reset(&calculator);
    calculator_focus = 0;
}

static void calculator_close(void)
{
}

static void move_focus(int amount)
{
    int next = calculator_focus + amount;

    next %= CP_CALCULATOR_KEY_COUNT;
    if(next < 0)
        next += CP_CALCULATOR_KEY_COUNT;
    calculator_focus = next;
}

static bool calculator_event(const struct cp_input_event *event)
{
    if(event == NULL || event->struct_size < sizeof(*event))
        return false;

    switch((enum cp_input_type)event->type) {
    case CP_INPUT_WHEEL_CLOCKWISE:
        move_focus(1);
        return true;
    case CP_INPUT_WHEEL_COUNTERCLOCKWISE:
        move_focus(-1);
        return true;
    case CP_INPUT_LEFT:
        move_focus(-1);
        return true;
    case CP_INPUT_RIGHT:
        move_focus(1);
        return true;
    case CP_INPUT_SELECT:
        if(!event->repeated)
            cp_calculator_press(
                &calculator,
                (enum cp_calculator_key)calculator_focus);
        return true;
    case CP_INPUT_PLAY:
        if(!event->repeated)
            cp_calculator_press(&calculator,
                                CP_CALCULATOR_KEY_EQUALS);
        return true;
    case CP_INPUT_MENU:
    default:
        return false;
    }
}

static bool calculator_tick(uint32_t epoch_seconds,
                            uint32_t monotonic_ms)
{
    (void)epoch_seconds;
    (void)monotonic_ms;
    return false;
}

static void render_key(struct cp_scene *scene, int index)
{
    int row = index / 4;
    int column = index % 4;
    int x = CALCULATOR_KEYBOARD_X +
            column * (CALCULATOR_KEY_WIDTH + CALCULATOR_COLUMN_GAP);
    int y = CALCULATOR_KEYBOARD_Y +
            row * (CALCULATOR_KEY_HEIGHT + CALCULATOR_ROW_GAP);
    bool focused = index == calculator_focus;
    struct cp_draw_command *rectangle;

    rectangle = add_rectangle(
        scene, x, y, CALCULATOR_KEY_WIDTH, CALCULATOR_KEY_HEIGHT,
        focused ? CP_COLOR_ACCENT : CP_COLOR_SURFACE_RAISED, 6);
    if(rectangle != NULL && focused) {
        rectangle->flags |= CP_DRAW_FOCUSED;
        rectangle->border = CP_COLOR_ACCENT;
        rectangle->border_opacity = 255;
        rectangle->border_width = 2;
    }

    add_text(scene, x, y, CALCULATOR_KEY_WIDTH,
             CALCULATOR_KEY_HEIGHT, CP_FONT_LABEL, CP_ALIGN_CENTER,
             focused ? CP_COLOR_ACCENT_FOREGROUND :
                       key_foreground(index),
             key_label(index));
}

static void calculator_render(struct cp_scene *scene)
{
    char expression[CP_MINIAPP_TEXT_SIZE];
    char result[CP_MINIAPP_TEXT_SIZE];
    enum cp_font_token result_font;
    int index;

    if(scene == NULL)
        return;

    cp_scene_reset(scene);
    scene->background = CP_COLOR_BACKGROUND;
    add_rectangle(scene, CALCULATOR_PANEL_X, CALCULATOR_PANEL_Y,
                  CALCULATOR_PANEL_WIDTH, CALCULATOR_PANEL_HEIGHT,
                  CP_COLOR_SURFACE, 12);

    format_expression(expression, sizeof(expression));
    add_text(scene, 24, 47, 272, 12, CP_FONT_LABEL,
             CP_ALIGN_RIGHT, CP_COLOR_MUTED, expression);

    format_current_value(result, sizeof(result));
    result_font = local_text_length(result) <= 8
        ? CP_FONT_DISPLAY : CP_FONT_NUMBER;
    add_text(scene, 24, 60, 272, 52, result_font,
             CP_ALIGN_RIGHT,
             calculator.error == CP_CALCULATOR_ERROR_NONE
                 ? CP_COLOR_WHITE : CP_COLOR_ERROR,
             result);
    if((calculator_host->capabilities & CP_CAP_DRAW_DIVIDER) != 0) {
        struct cp_draw_command *divider =
            cp_scene_add(scene, CP_DRAW_DIVIDER);

        if(divider != NULL) {
            divider->x = 24;
            divider->y = 111;
            divider->width = 272;
            divider->height = 1;
            divider->background = CP_COLOR_MUTED;
            divider->opacity = 72;
        }
    }

    for(index = 0; index < CP_CALCULATOR_KEY_COUNT; ++index)
        render_key(scene, index);
}

static const struct cp_miniapp_ops calculator_ops = {
    CP_MINIAPP_ABI_VERSION,
    sizeof(struct cp_miniapp_ops),
    "calculator",
    "Calculator",
    "1.2.0",
    calculator_open,
    calculator_close,
    calculator_event,
    calculator_tick,
    calculator_render
};

const struct cp_miniapp_ops *
cp_miniapp_entry(const struct cp_host_api *host)
{
    struct cp_system_info info;

    if(host == NULL ||
       host->abi_version != CP_MINIAPP_ABI_VERSION ||
       host->struct_size < sizeof(struct cp_host_api) ||
       host->format_number == NULL)
        return NULL;

    calculator_host = host;
    calculator_language = CP_LANGUAGE_ENGLISH;
    if(CP_HOST_HAS(host, CP_CAP_SYSTEM_INFO, system_info)) {
        info.struct_size = sizeof(info);
        if(host->system_info(&info) == 0 && info.language < CP_LANGUAGE_COUNT)
            calculator_language = info.language;
    }
    return &calculator_ops;
}

#ifdef CRAZYPOD_MINIAPP_PACKAGE
CP_MINIAPP_HEADER;
#endif
