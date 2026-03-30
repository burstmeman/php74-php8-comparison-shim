#include "p748_cmps_internal.h"

#include "zend_execute.h"

int p748_cmps_simulate_php8_result(
    zend_execute_data *execute_data,
    const zend_op *opline,
    zval *op1,
    zval *op2)
{
    zend_string *op1_str;
    zend_string *op2_str;
    int cmp;
    zval *result;

    /* Why: PHP 8 compares non-numeric string vs number as strings.
     * Caller (opcode handler) has already verified both type and result-differ conditions. */
    op1_str = zval_get_string(op1);
    op2_str = zval_get_string(op2);
    cmp = zend_binary_strcmp(
        ZSTR_VAL(op1_str), ZSTR_LEN(op1_str),
        ZSTR_VAL(op2_str), ZSTR_LEN(op2_str));

    result = EX_VAR(opline->result.var);

    switch (opline->opcode) {
        case ZEND_IS_EQUAL:
        case ZEND_CASE:
            ZVAL_BOOL(result, cmp == 0);
            break;
        case ZEND_IS_NOT_EQUAL:
            ZVAL_BOOL(result, cmp != 0);
            break;
        case ZEND_IS_SMALLER:
            ZVAL_BOOL(result, cmp < 0);
            break;
        case ZEND_IS_SMALLER_OR_EQUAL:
            ZVAL_BOOL(result, cmp <= 0);
            break;
        case ZEND_SPACESHIP:
            ZVAL_LONG(result, (cmp > 0) - (cmp < 0));
            break;
        default:
            zend_string_release(op1_str);
            zend_string_release(op2_str);
            return 0;
    }

    zend_string_release(op1_str);
    zend_string_release(op2_str);

    return 1;
}
