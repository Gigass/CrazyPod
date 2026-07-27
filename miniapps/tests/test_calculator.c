#include <stdio.h>
#include <string.h>

#include "../calculator/engine.h"
#include "../sdk/crazypod_miniapp.h"

static int failures;

extern const struct cp_miniapp_ops *
cp_miniapp_entry(const struct cp_host_api *host);

static void mock_format_number(double value, char *buffer,
                               size_t capacity)
{
    if(buffer == NULL || capacity == 0)
        return;
    snprintf(buffer, capacity, "%.15g", value);
}

static const struct cp_host_api mock_host = {
    .abi_version = CP_MINIAPP_ABI_VERSION,
    .struct_size = sizeof(struct cp_host_api),
    .format_number = mock_format_number,
    .capabilities = CP_CAP_DRAW_DIVIDER
};

#define EXPECT_TRUE(expression)                                           \
    do {                                                                  \
        if(!(expression)) {                                               \
            fprintf(stderr, "%s:%d: expected true: %s\n",                \
                    __FILE__, __LINE__, #expression);                      \
            ++failures;                                                   \
        }                                                                 \
    } while(0)

#define EXPECT_TEXT(actual, expected)                                     \
    do {                                                                  \
        const char *actual_value = (actual);                              \
        const char *expected_value = (expected);                          \
        if(strcmp(actual_value, expected_value) != 0) {                   \
            fprintf(stderr, "%s:%d: expected \"%s\", got \"%s\"\n",      \
                    __FILE__, __LINE__, expected_value, actual_value);     \
            ++failures;                                                   \
        }                                                                 \
    } while(0)

static double absolute_value(double value)
{
    return value < 0.0 ? -value : value;
}

static void expect_number(double actual, double expected)
{
    double scale = absolute_value(expected);
    double tolerance;

    if(scale < 1.0)
        scale = 1.0;
    tolerance = scale * 1e-12;
    if(absolute_value(actual - expected) > tolerance) {
        fprintf(stderr, "%s:%d: expected %.15g, got %.15g\n",
                __FILE__, __LINE__, expected, actual);
        ++failures;
    }
}

static void press(struct cp_calculator *calculator,
                  enum cp_calculator_key key)
{
    cp_calculator_press(calculator, key);
}

static struct cp_input_event input_event(enum cp_input_type type,
                                         unsigned steps,
                                         bool repeated)
{
    struct cp_input_event event;

    memset(&event, 0, sizeof(event));
    event.struct_size = sizeof(event);
    event.type = (uint8_t)type;
    event.steps = (uint8_t)steps;
    event.repeated = repeated ? 1 : 0;
    return event;
}

static const struct cp_draw_command *
scene_find(const struct cp_scene *scene, enum cp_draw_type type,
           int x, int y)
{
    unsigned index;

    for(index = 0; index < scene->command_count; ++index) {
        const struct cp_draw_command *command =
            &scene->commands[index];

        if(command->type == type &&
           command->x == x && command->y == y)
            return command;
    }
    return NULL;
}

static bool scene_has_text(const struct cp_scene *scene,
                           const char *text)
{
    unsigned index;

    for(index = 0; index < scene->command_count; ++index) {
        const struct cp_draw_command *command =
            &scene->commands[index];

        if(command->type == CP_DRAW_TEXT &&
           strcmp(command->text, text) == 0)
            return true;
    }
    return false;
}

static void test_entry_sign_decimal_and_backspace(void)
{
    struct cp_calculator calculator;

    cp_calculator_reset(&calculator);
    EXPECT_TRUE(cp_calculator_clear_is_all(&calculator));

    press(&calculator, CP_CALCULATOR_KEY_1);
    press(&calculator, CP_CALCULATOR_KEY_DECIMAL);
    press(&calculator, CP_CALCULATOR_KEY_2);
    press(&calculator, CP_CALCULATOR_KEY_0);
    press(&calculator, CP_CALCULATOR_KEY_DECIMAL);
    EXPECT_TEXT(cp_calculator_entry_text(&calculator), "1.20");
    expect_number(calculator.display_value, 1.2);
    EXPECT_TRUE(!cp_calculator_clear_is_all(&calculator));

    press(&calculator, CP_CALCULATOR_KEY_SIGN);
    EXPECT_TEXT(cp_calculator_entry_text(&calculator), "-1.20");
    expect_number(calculator.display_value, -1.2);

    press(&calculator, CP_CALCULATOR_KEY_BACKSPACE);
    EXPECT_TEXT(cp_calculator_entry_text(&calculator), "-1.2");
    press(&calculator, CP_CALCULATOR_KEY_BACKSPACE);
    EXPECT_TEXT(cp_calculator_entry_text(&calculator), "-1.");
    press(&calculator, CP_CALCULATOR_KEY_BACKSPACE);
    EXPECT_TEXT(cp_calculator_entry_text(&calculator), "-1");
    press(&calculator, CP_CALCULATOR_KEY_BACKSPACE);
    EXPECT_TEXT(cp_calculator_entry_text(&calculator), "0");
    expect_number(calculator.display_value, 0.0);
}

static void test_chained_operations_and_operator_replacement(void)
{
    struct cp_calculator calculator;

    cp_calculator_reset(&calculator);
    press(&calculator, CP_CALCULATOR_KEY_2);
    press(&calculator, CP_CALCULATOR_KEY_ADD);
    press(&calculator, CP_CALCULATOR_KEY_3);
    press(&calculator, CP_CALCULATOR_KEY_MULTIPLY);
    expect_number(calculator.display_value, 5.0);
    press(&calculator, CP_CALCULATOR_KEY_4);
    press(&calculator, CP_CALCULATOR_KEY_EQUALS);
    expect_number(calculator.display_value, 20.0);
    press(&calculator, CP_CALCULATOR_KEY_EQUALS);
    expect_number(calculator.display_value, 80.0);

    cp_calculator_reset(&calculator);
    press(&calculator, CP_CALCULATOR_KEY_8);
    press(&calculator, CP_CALCULATOR_KEY_ADD);
    press(&calculator, CP_CALCULATOR_KEY_MULTIPLY);
    press(&calculator, CP_CALCULATOR_KEY_2);
    press(&calculator, CP_CALCULATOR_KEY_EQUALS);
    expect_number(calculator.display_value, 16.0);
}

static void test_repeated_equals(void)
{
    struct cp_calculator calculator;

    cp_calculator_reset(&calculator);
    press(&calculator, CP_CALCULATOR_KEY_2);
    press(&calculator, CP_CALCULATOR_KEY_ADD);
    press(&calculator, CP_CALCULATOR_KEY_3);
    press(&calculator, CP_CALCULATOR_KEY_EQUALS);
    expect_number(calculator.display_value, 5.0);
    press(&calculator, CP_CALCULATOR_KEY_EQUALS);
    expect_number(calculator.display_value, 8.0);
    press(&calculator, CP_CALCULATOR_KEY_EQUALS);
    expect_number(calculator.display_value, 11.0);

    cp_calculator_reset(&calculator);
    press(&calculator, CP_CALCULATOR_KEY_5);
    press(&calculator, CP_CALCULATOR_KEY_ADD);
    press(&calculator, CP_CALCULATOR_KEY_EQUALS);
    expect_number(calculator.display_value, 10.0);
    press(&calculator, CP_CALCULATOR_KEY_EQUALS);
    expect_number(calculator.display_value, 15.0);
}

static void enter_200(struct cp_calculator *calculator)
{
    press(calculator, CP_CALCULATOR_KEY_2);
    press(calculator, CP_CALCULATOR_KEY_0);
    press(calculator, CP_CALCULATOR_KEY_0);
}

static void enter_10_percent(struct cp_calculator *calculator)
{
    press(calculator, CP_CALCULATOR_KEY_1);
    press(calculator, CP_CALCULATOR_KEY_0);
    press(calculator, CP_CALCULATOR_KEY_PERCENT);
}

static void test_percent_semantics(void)
{
    struct cp_calculator calculator;

    cp_calculator_reset(&calculator);
    enter_200(&calculator);
    press(&calculator, CP_CALCULATOR_KEY_ADD);
    enter_10_percent(&calculator);
    expect_number(calculator.display_value, 20.0);
    press(&calculator, CP_CALCULATOR_KEY_EQUALS);
    expect_number(calculator.display_value, 220.0);

    cp_calculator_reset(&calculator);
    enter_200(&calculator);
    press(&calculator, CP_CALCULATOR_KEY_SUBTRACT);
    enter_10_percent(&calculator);
    press(&calculator, CP_CALCULATOR_KEY_EQUALS);
    expect_number(calculator.display_value, 180.0);

    cp_calculator_reset(&calculator);
    enter_200(&calculator);
    press(&calculator, CP_CALCULATOR_KEY_MULTIPLY);
    enter_10_percent(&calculator);
    press(&calculator, CP_CALCULATOR_KEY_EQUALS);
    expect_number(calculator.display_value, 20.0);

    cp_calculator_reset(&calculator);
    enter_200(&calculator);
    press(&calculator, CP_CALCULATOR_KEY_DIVIDE);
    enter_10_percent(&calculator);
    press(&calculator, CP_CALCULATOR_KEY_EQUALS);
    expect_number(calculator.display_value, 2000.0);

    cp_calculator_reset(&calculator);
    press(&calculator, CP_CALCULATOR_KEY_5);
    press(&calculator, CP_CALCULATOR_KEY_0);
    press(&calculator, CP_CALCULATOR_KEY_PERCENT);
    expect_number(calculator.display_value, 0.5);
}

static void test_negative_operand(void)
{
    struct cp_calculator calculator;

    cp_calculator_reset(&calculator);
    press(&calculator, CP_CALCULATOR_KEY_8);
    press(&calculator, CP_CALCULATOR_KEY_ADD);
    press(&calculator, CP_CALCULATOR_KEY_SIGN);
    EXPECT_TEXT(cp_calculator_entry_text(&calculator), "-0");
    press(&calculator, CP_CALCULATOR_KEY_2);
    press(&calculator, CP_CALCULATOR_KEY_EQUALS);
    expect_number(calculator.display_value, 6.0);

    cp_calculator_reset(&calculator);
    press(&calculator, CP_CALCULATOR_KEY_SIGN);
    EXPECT_TEXT(cp_calculator_entry_text(&calculator), "-0");
    press(&calculator, CP_CALCULATOR_KEY_3);
    expect_number(calculator.display_value, -3.0);
}

static void test_contextual_clear(void)
{
    struct cp_calculator calculator;

    cp_calculator_reset(&calculator);
    press(&calculator, CP_CALCULATOR_KEY_1);
    press(&calculator, CP_CALCULATOR_KEY_2);
    press(&calculator, CP_CALCULATOR_KEY_ADD);
    EXPECT_TRUE(cp_calculator_clear_is_all(&calculator));

    press(&calculator, CP_CALCULATOR_KEY_3);
    press(&calculator, CP_CALCULATOR_KEY_4);
    EXPECT_TRUE(!cp_calculator_clear_is_all(&calculator));
    press(&calculator, CP_CALCULATOR_KEY_CLEAR);
    EXPECT_TRUE(cp_calculator_clear_is_all(&calculator));
    press(&calculator, CP_CALCULATOR_KEY_5);
    press(&calculator, CP_CALCULATOR_KEY_EQUALS);
    expect_number(calculator.display_value, 17.0);

    press(&calculator, CP_CALCULATOR_KEY_CLEAR);
    expect_number(calculator.display_value, 0.0);
    EXPECT_TRUE(cp_calculator_clear_is_all(&calculator));

    cp_calculator_reset(&calculator);
    press(&calculator, CP_CALCULATOR_KEY_9);
    press(&calculator, CP_CALCULATOR_KEY_MULTIPLY);
    press(&calculator, CP_CALCULATOR_KEY_CLEAR);
    press(&calculator, CP_CALCULATOR_KEY_2);
    press(&calculator, CP_CALCULATOR_KEY_EQUALS);
    expect_number(calculator.display_value, 2.0);
}

static void test_divide_by_zero_recovery(void)
{
    struct cp_calculator calculator;

    cp_calculator_reset(&calculator);
    press(&calculator, CP_CALCULATOR_KEY_8);
    press(&calculator, CP_CALCULATOR_KEY_DIVIDE);
    press(&calculator, CP_CALCULATOR_KEY_0);
    press(&calculator, CP_CALCULATOR_KEY_EQUALS);
    EXPECT_TRUE(calculator.error ==
                CP_CALCULATOR_ERROR_DIVIDE_BY_ZERO);
    EXPECT_TRUE(cp_calculator_clear_is_all(&calculator));

    press(&calculator, CP_CALCULATOR_KEY_ADD);
    EXPECT_TRUE(calculator.error ==
                CP_CALCULATOR_ERROR_DIVIDE_BY_ZERO);
    press(&calculator, CP_CALCULATOR_KEY_2);
    EXPECT_TRUE(calculator.error == CP_CALCULATOR_ERROR_NONE);
    expect_number(calculator.display_value, 2.0);
    press(&calculator, CP_CALCULATOR_KEY_ADD);
    press(&calculator, CP_CALCULATOR_KEY_3);
    press(&calculator, CP_CALCULATOR_KEY_EQUALS);
    expect_number(calculator.display_value, 5.0);

    cp_calculator_reset(&calculator);
    press(&calculator, CP_CALCULATOR_KEY_1);
    press(&calculator, CP_CALCULATOR_KEY_DIVIDE);
    press(&calculator, CP_CALCULATOR_KEY_0);
    press(&calculator, CP_CALCULATOR_KEY_EQUALS);
    press(&calculator, CP_CALCULATOR_KEY_BACKSPACE);
    EXPECT_TRUE(calculator.error == CP_CALCULATOR_ERROR_NONE);
    expect_number(calculator.display_value, 0.0);
}

static void test_scene_a_and_row_major_focus(void)
{
    const struct cp_miniapp_ops *ops =
        cp_miniapp_entry(&mock_host);
    const struct cp_draw_command *command;
    struct cp_input_event event;
    struct cp_scene scene;
    int index;

    EXPECT_TRUE(ops != NULL);
    if(ops == NULL)
        return;
    EXPECT_TEXT(ops->id, "calculator");
    ops->open();
    ops->render(&scene);

    EXPECT_TRUE(scene.struct_size == sizeof(scene));
    EXPECT_TRUE(scene.background == CP_COLOR_BACKGROUND);
    EXPECT_TRUE(scene.command_count == 44);

    command = scene_find(&scene, CP_DRAW_RECT, 10, 40);
    EXPECT_TRUE(command != NULL);
    if(command != NULL) {
        EXPECT_TRUE(command->width == 300);
        EXPECT_TRUE(command->height == 188);
        EXPECT_TRUE(command->radius == 12);
        EXPECT_TRUE(command->background == CP_COLOR_SURFACE);
    }

    command = scene_find(&scene, CP_DRAW_TEXT, 24, 47);
    EXPECT_TRUE(command != NULL);
    if(command != NULL) {
        EXPECT_TRUE(command->width == 272);
        EXPECT_TRUE(command->font == CP_FONT_LABEL);
        EXPECT_TRUE(command->align == CP_ALIGN_RIGHT);
        EXPECT_TRUE(command->foreground == CP_COLOR_MUTED);
    }

    command = scene_find(&scene, CP_DRAW_TEXT, 24, 60);
    EXPECT_TRUE(command != NULL);
    if(command != NULL) {
        EXPECT_TRUE(command->width == 272);
        EXPECT_TRUE(command->height == 52);
        EXPECT_TRUE(command->font == CP_FONT_DISPLAY);
        EXPECT_TRUE(command->align == CP_ALIGN_RIGHT);
        EXPECT_TEXT(command->text, "0");
    }
    command = scene_find(&scene, CP_DRAW_DIVIDER, 24, 111);
    EXPECT_TRUE(command != NULL);
    if(command != NULL) {
        EXPECT_TRUE(command->width == 272);
        EXPECT_TRUE(command->height == 1);
        EXPECT_TRUE(command->background == CP_COLOR_MUTED);
    }

    command = scene_find(&scene, CP_DRAW_RECT, 16, 116);
    EXPECT_TRUE(command != NULL);
    if(command != NULL) {
        EXPECT_TRUE(command->width == 69);
        EXPECT_TRUE(command->height == 20);
        EXPECT_TRUE(command->radius == 6);
        EXPECT_TRUE(command->background == CP_COLOR_ACCENT);
        EXPECT_TRUE((command->flags & CP_DRAW_FOCUSED) != 0);
        EXPECT_TRUE(command->border_opacity == 255);
    }
    EXPECT_TRUE(scene_has_text(&scene, "AC"));
    EXPECT_TRUE(scene_has_text(&scene, "\xc2\xb1"));
    EXPECT_TRUE(scene_has_text(&scene, "\xc3\xb7"));
    EXPECT_TRUE(scene_has_text(&scene, "\xc3\x97"));
    EXPECT_TRUE(scene_has_text(&scene, "\xe2\x88\x92"));
    EXPECT_TRUE(scene_has_text(&scene, "\xe2\x8c\xab"));

    command = scene_find(&scene, CP_DRAW_RECT, 235, 204);
    EXPECT_TRUE(command != NULL);

    event = input_event(CP_INPUT_WHEEL_CLOCKWISE, 5, false);
    EXPECT_TRUE(ops->event(&event));
    ops->render(&scene);
    command = scene_find(&scene, CP_DRAW_RECT, 89, 116);
    EXPECT_TRUE(command != NULL);
    if(command != NULL) {
        EXPECT_TRUE(command->background == CP_COLOR_ACCENT);
        EXPECT_TRUE((command->flags & CP_DRAW_FOCUSED) != 0);
    }

    event.steps = 1;
    for(index = 0; index < 4; ++index)
        EXPECT_TRUE(ops->event(&event));
    ops->render(&scene);
    command = scene_find(&scene, CP_DRAW_RECT, 89, 138);
    EXPECT_TRUE(command != NULL);
    if(command != NULL)
        EXPECT_TRUE((command->flags & CP_DRAW_FOCUSED) != 0);

    event = input_event(CP_INPUT_SELECT, 1, false);
    EXPECT_TRUE(ops->event(&event));
    ops->render(&scene);
    EXPECT_TRUE(scene_has_text(&scene, "8"));
    EXPECT_TRUE(scene_has_text(&scene, "C"));

    event = input_event(CP_INPUT_WHEEL_CLOCKWISE, 1, false);
    for(index = 0; index < 10; ++index)
        EXPECT_TRUE(ops->event(&event));
    event = input_event(CP_INPUT_SELECT, 1, false);
    EXPECT_TRUE(ops->event(&event));
    event = input_event(CP_INPUT_WHEEL_CLOCKWISE, 1, false);
    for(index = 0; index < 10; ++index)
        EXPECT_TRUE(ops->event(&event));
    event = input_event(CP_INPUT_SELECT, 1, false);
    EXPECT_TRUE(ops->event(&event));
    event = input_event(CP_INPUT_PLAY, 1, false);
    EXPECT_TRUE(ops->event(&event));
    ops->render(&scene);
    EXPECT_TRUE(scene_has_text(&scene, "16"));

    event = input_event(CP_INPUT_SELECT, 1, true);
    EXPECT_TRUE(ops->event(&event));
    ops->render(&scene);
    EXPECT_TRUE(scene_has_text(&scene, "16"));

    event = input_event(CP_INPUT_WHEEL_COUNTERCLOCKWISE, 6, false);
    EXPECT_TRUE(ops->event(&event));
    ops->render(&scene);
    command = scene_find(&scene, CP_DRAW_RECT, 16, 138);
    EXPECT_TRUE(command != NULL);
    if(command != NULL)
        EXPECT_TRUE(command->background == CP_COLOR_ACCENT);

    ops->open();
    event = input_event(CP_INPUT_WHEEL_COUNTERCLOCKWISE, 6, false);
    EXPECT_TRUE(ops->event(&event));
    ops->render(&scene);
    command = scene_find(&scene, CP_DRAW_RECT, 235, 204);
    EXPECT_TRUE(command != NULL);
    if(command != NULL)
        EXPECT_TRUE((command->flags & CP_DRAW_FOCUSED) != 0);

    ops->open();
    event = input_event(CP_INPUT_WHEEL_CLOCKWISE, 4, false);
    EXPECT_TRUE(ops->event(&event));
    event.steps = 1;
    for(index = 0; index < 5; ++index)
        EXPECT_TRUE(ops->event(&event));
    event = input_event(CP_INPUT_SELECT, 1, false);
    for(index = 0; index < 9; ++index)
        EXPECT_TRUE(ops->event(&event));
    ops->render(&scene);
    command = scene_find(&scene, CP_DRAW_TEXT, 24, 60);
    EXPECT_TRUE(command != NULL);
    if(command != NULL) {
        EXPECT_TEXT(command->text, "999999999");
        EXPECT_TRUE(command->font == CP_FONT_NUMBER);
    }

    event = input_event(CP_INPUT_MENU, 1, false);
    EXPECT_TRUE(!ops->event(&event));
    ops->close();
}

int main(void)
{
    test_entry_sign_decimal_and_backspace();
    test_chained_operations_and_operator_replacement();
    test_repeated_equals();
    test_percent_semantics();
    test_negative_operand();
    test_contextual_clear();
    test_divide_by_zero_recovery();
    test_scene_a_and_row_major_focus();

    if(failures != 0) {
        fprintf(stderr, "calculator tests: %d failure(s)\n", failures);
        return 1;
    }
    puts("calculator tests: OK");
    return 0;
}
