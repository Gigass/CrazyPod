#ifndef CRAZYPOD_CALCULATOR_ENGINE_H
#define CRAZYPOD_CALCULATOR_ENGINE_H

#include <stdbool.h>
#include <stddef.h>

#define CP_CALCULATOR_ENTRY_CAPACITY 32
#define CP_CALCULATOR_KEY_COUNT 20

enum cp_calculator_key {
    CP_CALCULATOR_KEY_CLEAR = 0,
    CP_CALCULATOR_KEY_SIGN,
    CP_CALCULATOR_KEY_PERCENT,
    CP_CALCULATOR_KEY_DIVIDE,
    CP_CALCULATOR_KEY_7,
    CP_CALCULATOR_KEY_8,
    CP_CALCULATOR_KEY_9,
    CP_CALCULATOR_KEY_MULTIPLY,
    CP_CALCULATOR_KEY_4,
    CP_CALCULATOR_KEY_5,
    CP_CALCULATOR_KEY_6,
    CP_CALCULATOR_KEY_SUBTRACT,
    CP_CALCULATOR_KEY_1,
    CP_CALCULATOR_KEY_2,
    CP_CALCULATOR_KEY_3,
    CP_CALCULATOR_KEY_ADD,
    CP_CALCULATOR_KEY_0,
    CP_CALCULATOR_KEY_DECIMAL,
    CP_CALCULATOR_KEY_BACKSPACE,
    CP_CALCULATOR_KEY_EQUALS
};

enum cp_calculator_operator {
    CP_CALCULATOR_OPERATOR_NONE = 0,
    CP_CALCULATOR_OPERATOR_ADD,
    CP_CALCULATOR_OPERATOR_SUBTRACT,
    CP_CALCULATOR_OPERATOR_MULTIPLY,
    CP_CALCULATOR_OPERATOR_DIVIDE
};

enum cp_calculator_error {
    CP_CALCULATOR_ERROR_NONE = 0,
    CP_CALCULATOR_ERROR_DIVIDE_BY_ZERO,
    CP_CALCULATOR_ERROR_RANGE
};

struct cp_calculator {
    double display_value;
    double accumulator;
    double repeat_operand;
    double last_left_operand;
    char entry[CP_CALCULATOR_ENTRY_CAPACITY];
    enum cp_calculator_operator pending_operator;
    enum cp_calculator_operator repeat_operator;
    enum cp_calculator_error error;
    bool has_pending_operator;
    bool right_operand_ready;
    bool entering;
    bool just_evaluated;
};

void cp_calculator_reset(struct cp_calculator *calculator);
void cp_calculator_press(struct cp_calculator *calculator,
                         enum cp_calculator_key key);

bool cp_calculator_clear_is_all(const struct cp_calculator *calculator);
const char *cp_calculator_entry_text(
    const struct cp_calculator *calculator);
const char *cp_calculator_operator_symbol(
    enum cp_calculator_operator operation);

#endif
