#include "engine.h"

#include <float.h>

#define CP_CALCULATOR_MAX_DIGITS 15

static size_t text_length(const char *text)
{
    size_t length = 0;

    if(text == NULL)
        return 0;
    while(text[length] != '\0')
        ++length;
    return length;
}

static void set_zero_entry(struct cp_calculator *calculator)
{
    calculator->entry[0] = '0';
    calculator->entry[1] = '\0';
}

static bool text_has_decimal(const char *text)
{
    size_t index;

    for(index = 0; text[index] != '\0'; ++index)
        if(text[index] == '.')
            return true;
    return false;
}

static int text_digit_count(const char *text)
{
    int count = 0;
    size_t index;

    for(index = 0; text[index] != '\0'; ++index)
        if(text[index] >= '0' && text[index] <= '9')
            ++count;
    return count;
}

static double parse_entry(const char *text)
{
    bool negative = false;
    bool fractional = false;
    double value = 0.0;
    double place = 0.1;
    size_t index = 0;

    if(text[0] == '-') {
        negative = true;
        index = 1;
    }

    for(; text[index] != '\0'; ++index) {
        char character = text[index];

        if(character == '.') {
            fractional = true;
            continue;
        }
        if(character < '0' || character > '9')
            continue;
        if(fractional) {
            value += (double)(character - '0') * place;
            place *= 0.1;
        } else {
            value = value * 10.0 + (double)(character - '0');
        }
    }
    return negative ? -value : value;
}

static bool value_is_valid(double value)
{
    return value == value && value <= DBL_MAX && value >= -DBL_MAX;
}

static void set_error(struct cp_calculator *calculator,
                      enum cp_calculator_error error)
{
    calculator->display_value = 0.0;
    calculator->accumulator = 0.0;
    calculator->repeat_operand = 0.0;
    calculator->last_left_operand = 0.0;
    calculator->pending_operator = CP_CALCULATOR_OPERATOR_NONE;
    calculator->repeat_operator = CP_CALCULATOR_OPERATOR_NONE;
    calculator->error = error;
    calculator->has_pending_operator = false;
    calculator->right_operand_ready = false;
    calculator->entering = false;
    calculator->just_evaluated = false;
    set_zero_entry(calculator);
}

static bool calculate(enum cp_calculator_operator operation,
                      double left, double right, double *result,
                      enum cp_calculator_error *error)
{
    double value;

    switch(operation) {
    case CP_CALCULATOR_OPERATOR_ADD:
        value = left + right;
        break;
    case CP_CALCULATOR_OPERATOR_SUBTRACT:
        value = left - right;
        break;
    case CP_CALCULATOR_OPERATOR_MULTIPLY:
        value = left * right;
        break;
    case CP_CALCULATOR_OPERATOR_DIVIDE:
        if(right == 0.0) {
            *error = CP_CALCULATOR_ERROR_DIVIDE_BY_ZERO;
            return false;
        }
        value = left / right;
        break;
    case CP_CALCULATOR_OPERATOR_NONE:
    default:
        value = right;
        break;
    }

    if(!value_is_valid(value)) {
        *error = CP_CALCULATOR_ERROR_RANGE;
        return false;
    }
    *result = value;
    *error = CP_CALCULATOR_ERROR_NONE;
    return true;
}

void cp_calculator_reset(struct cp_calculator *calculator)
{
    if(calculator == NULL)
        return;

    calculator->display_value = 0.0;
    calculator->accumulator = 0.0;
    calculator->repeat_operand = 0.0;
    calculator->last_left_operand = 0.0;
    calculator->pending_operator = CP_CALCULATOR_OPERATOR_NONE;
    calculator->repeat_operator = CP_CALCULATOR_OPERATOR_NONE;
    calculator->error = CP_CALCULATOR_ERROR_NONE;
    calculator->has_pending_operator = false;
    calculator->right_operand_ready = false;
    calculator->entering = false;
    calculator->just_evaluated = false;
    set_zero_entry(calculator);
}

static void begin_entry(struct cp_calculator *calculator)
{
    if(calculator->just_evaluated &&
       !calculator->has_pending_operator)
        cp_calculator_reset(calculator);

    if(calculator->entering)
        return;

    set_zero_entry(calculator);
    calculator->display_value = 0.0;
    calculator->entering = true;
    calculator->just_evaluated = false;
    calculator->repeat_operator = CP_CALCULATOR_OPERATOR_NONE;
    if(calculator->has_pending_operator)
        calculator->right_operand_ready = true;
}

static void press_digit(struct cp_calculator *calculator, int digit)
{
    size_t length;

    if(calculator->error != CP_CALCULATOR_ERROR_NONE)
        cp_calculator_reset(calculator);
    begin_entry(calculator);

    length = text_length(calculator->entry);
    if(calculator->entry[0] == '0' &&
       calculator->entry[1] == '\0') {
        if(digit != 0)
            calculator->entry[0] = (char)('0' + digit);
    } else if(calculator->entry[0] == '-' &&
              calculator->entry[1] == '0' &&
              calculator->entry[2] == '\0') {
        if(digit != 0) {
            calculator->entry[1] = (char)('0' + digit);
            calculator->entry[2] = '\0';
        }
    } else if(text_digit_count(calculator->entry) <
                  CP_CALCULATOR_MAX_DIGITS &&
              length + 1 < CP_CALCULATOR_ENTRY_CAPACITY) {
        calculator->entry[length] = (char)('0' + digit);
        calculator->entry[length + 1] = '\0';
    }
    calculator->display_value = parse_entry(calculator->entry);
}

static void press_decimal(struct cp_calculator *calculator)
{
    size_t length;

    if(calculator->error != CP_CALCULATOR_ERROR_NONE)
        cp_calculator_reset(calculator);
    begin_entry(calculator);

    if(text_has_decimal(calculator->entry))
        return;
    length = text_length(calculator->entry);
    if(length + 1 >= CP_CALCULATOR_ENTRY_CAPACITY)
        return;
    calculator->entry[length] = '.';
    calculator->entry[length + 1] = '\0';
}

static void press_sign(struct cp_calculator *calculator)
{
    size_t length;
    size_t index;

    if(calculator->error != CP_CALCULATOR_ERROR_NONE)
        return;

    if((calculator->has_pending_operator &&
        !calculator->right_operand_ready) ||
       (!calculator->has_pending_operator &&
        !calculator->just_evaluated &&
        calculator->display_value == 0.0))
        begin_entry(calculator);

    if(calculator->entering) {
        length = text_length(calculator->entry);
        if(calculator->entry[0] == '-') {
            for(index = 0; index < length; ++index)
                calculator->entry[index] = calculator->entry[index + 1];
        } else if(length + 1 < CP_CALCULATOR_ENTRY_CAPACITY) {
            for(index = length + 1; index > 0; --index)
                calculator->entry[index] = calculator->entry[index - 1];
            calculator->entry[0] = '-';
        }
        calculator->display_value = parse_entry(calculator->entry);
    } else {
        calculator->display_value = -calculator->display_value;
        calculator->just_evaluated = false;
        calculator->repeat_operator = CP_CALCULATOR_OPERATOR_NONE;
        if(calculator->has_pending_operator)
            calculator->right_operand_ready = true;
    }
}

static void press_percent(struct cp_calculator *calculator)
{
    double operand;
    double value;

    if(calculator->error != CP_CALCULATOR_ERROR_NONE)
        return;

    if(calculator->has_pending_operator) {
        operand = calculator->right_operand_ready
            ? calculator->display_value : calculator->accumulator;
        if(calculator->pending_operator ==
               CP_CALCULATOR_OPERATOR_ADD ||
           calculator->pending_operator ==
               CP_CALCULATOR_OPERATOR_SUBTRACT)
            value = calculator->accumulator * operand / 100.0;
        else
            value = operand / 100.0;
        calculator->right_operand_ready = true;
    } else {
        value = calculator->display_value / 100.0;
        calculator->repeat_operator = CP_CALCULATOR_OPERATOR_NONE;
    }

    if(!value_is_valid(value)) {
        set_error(calculator, CP_CALCULATOR_ERROR_RANGE);
        return;
    }
    calculator->display_value = value;
    calculator->entering = false;
    calculator->just_evaluated = false;
}

static void press_operator(struct cp_calculator *calculator,
                           enum cp_calculator_operator operation)
{
    double result;
    enum cp_calculator_error error;

    if(calculator->error != CP_CALCULATOR_ERROR_NONE)
        return;

    if(calculator->has_pending_operator) {
        if(!calculator->right_operand_ready) {
            calculator->pending_operator = operation;
            return;
        }
        if(!calculate(calculator->pending_operator,
                      calculator->accumulator,
                      calculator->display_value, &result, &error)) {
            set_error(calculator, error);
            return;
        }
        calculator->display_value = result;
        calculator->accumulator = result;
    } else {
        calculator->accumulator = calculator->display_value;
    }

    calculator->pending_operator = operation;
    calculator->repeat_operator = CP_CALCULATOR_OPERATOR_NONE;
    calculator->has_pending_operator = true;
    calculator->right_operand_ready = false;
    calculator->entering = false;
    calculator->just_evaluated = false;
}

static void press_equals(struct cp_calculator *calculator)
{
    enum cp_calculator_operator operation;
    enum cp_calculator_error error;
    double left;
    double right;
    double result;

    if(calculator->error != CP_CALCULATOR_ERROR_NONE)
        return;

    if(calculator->has_pending_operator) {
        operation = calculator->pending_operator;
        left = calculator->accumulator;
        right = calculator->right_operand_ready
            ? calculator->display_value : calculator->accumulator;
    } else if(calculator->repeat_operator !=
              CP_CALCULATOR_OPERATOR_NONE) {
        operation = calculator->repeat_operator;
        left = calculator->display_value;
        right = calculator->repeat_operand;
    } else {
        calculator->entering = false;
        calculator->just_evaluated = true;
        return;
    }

    if(!calculate(operation, left, right, &result, &error)) {
        set_error(calculator, error);
        return;
    }

    calculator->last_left_operand = left;
    calculator->repeat_operand = right;
    calculator->repeat_operator = operation;
    calculator->pending_operator = CP_CALCULATOR_OPERATOR_NONE;
    calculator->display_value = result;
    calculator->has_pending_operator = false;
    calculator->right_operand_ready = false;
    calculator->entering = false;
    calculator->just_evaluated = true;
}

bool cp_calculator_clear_is_all(const struct cp_calculator *calculator)
{
    if(calculator == NULL ||
       calculator->error != CP_CALCULATOR_ERROR_NONE)
        return true;
    if(calculator->has_pending_operator)
        return !calculator->right_operand_ready &&
               !calculator->entering;
    if(calculator->entering)
        return false;
    return calculator->display_value == 0.0 &&
           !calculator->just_evaluated;
}

static void press_clear(struct cp_calculator *calculator)
{
    if(cp_calculator_clear_is_all(calculator)) {
        cp_calculator_reset(calculator);
        return;
    }

    calculator->display_value = 0.0;
    calculator->repeat_operator = CP_CALCULATOR_OPERATOR_NONE;
    calculator->entering = false;
    calculator->just_evaluated = false;
    set_zero_entry(calculator);
    if(calculator->has_pending_operator) {
        calculator->right_operand_ready = false;
    } else {
        calculator->accumulator = 0.0;
        calculator->repeat_operand = 0.0;
        calculator->last_left_operand = 0.0;
    }
}

static void press_backspace(struct cp_calculator *calculator)
{
    size_t length;

    if(calculator->error != CP_CALCULATOR_ERROR_NONE) {
        cp_calculator_reset(calculator);
        return;
    }
    if(!calculator->entering)
        return;

    length = text_length(calculator->entry);
    if(length <= 1 ||
       (length == 2 && calculator->entry[0] == '-')) {
        set_zero_entry(calculator);
    } else {
        calculator->entry[length - 1] = '\0';
    }
    calculator->display_value = parse_entry(calculator->entry);
    calculator->just_evaluated = false;
}

static enum cp_calculator_operator key_operator(
    enum cp_calculator_key key)
{
    switch(key) {
    case CP_CALCULATOR_KEY_ADD:
        return CP_CALCULATOR_OPERATOR_ADD;
    case CP_CALCULATOR_KEY_SUBTRACT:
        return CP_CALCULATOR_OPERATOR_SUBTRACT;
    case CP_CALCULATOR_KEY_MULTIPLY:
        return CP_CALCULATOR_OPERATOR_MULTIPLY;
    case CP_CALCULATOR_KEY_DIVIDE:
        return CP_CALCULATOR_OPERATOR_DIVIDE;
    default:
        return CP_CALCULATOR_OPERATOR_NONE;
    }
}

void cp_calculator_press(struct cp_calculator *calculator,
                         enum cp_calculator_key key)
{
    enum cp_calculator_operator operation;

    if(calculator == NULL)
        return;

    switch(key) {
    case CP_CALCULATOR_KEY_0:
        press_digit(calculator, 0);
        break;
    case CP_CALCULATOR_KEY_1:
        press_digit(calculator, 1);
        break;
    case CP_CALCULATOR_KEY_2:
        press_digit(calculator, 2);
        break;
    case CP_CALCULATOR_KEY_3:
        press_digit(calculator, 3);
        break;
    case CP_CALCULATOR_KEY_4:
        press_digit(calculator, 4);
        break;
    case CP_CALCULATOR_KEY_5:
        press_digit(calculator, 5);
        break;
    case CP_CALCULATOR_KEY_6:
        press_digit(calculator, 6);
        break;
    case CP_CALCULATOR_KEY_7:
        press_digit(calculator, 7);
        break;
    case CP_CALCULATOR_KEY_8:
        press_digit(calculator, 8);
        break;
    case CP_CALCULATOR_KEY_9:
        press_digit(calculator, 9);
        break;
    case CP_CALCULATOR_KEY_DECIMAL:
        press_decimal(calculator);
        break;
    case CP_CALCULATOR_KEY_SIGN:
        press_sign(calculator);
        break;
    case CP_CALCULATOR_KEY_PERCENT:
        press_percent(calculator);
        break;
    case CP_CALCULATOR_KEY_EQUALS:
        press_equals(calculator);
        break;
    case CP_CALCULATOR_KEY_CLEAR:
        press_clear(calculator);
        break;
    case CP_CALCULATOR_KEY_BACKSPACE:
        press_backspace(calculator);
        break;
    default:
        operation = key_operator(key);
        if(operation != CP_CALCULATOR_OPERATOR_NONE)
            press_operator(calculator, operation);
        break;
    }
}

const char *cp_calculator_entry_text(
    const struct cp_calculator *calculator)
{
    return calculator != NULL ? calculator->entry : "0";
}

const char *cp_calculator_operator_symbol(
    enum cp_calculator_operator operation)
{
    switch(operation) {
    case CP_CALCULATOR_OPERATOR_ADD:
        return "+";
    case CP_CALCULATOR_OPERATOR_SUBTRACT:
        return "\xe2\x88\x92";
    case CP_CALCULATOR_OPERATOR_MULTIPLY:
        return "\xc3\x97";
    case CP_CALCULATOR_OPERATOR_DIVIDE:
        return "\xc3\xb7";
    case CP_CALCULATOR_OPERATOR_NONE:
    default:
        return "";
    }
}
