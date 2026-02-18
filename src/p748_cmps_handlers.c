#include "p748_cmps_internal.h"

#include "zend_execute.h"
#include "zend_exceptions.h"

typedef struct {
    zend_uchar opcode;
    user_opcode_handler_t previous;
} p748_cmps_opcode_handler_entry;

static int p748_cmps_opcode_handler(zend_execute_data *execute_data);

static p748_cmps_opcode_handler_entry p748_cmps_opcode_handlers[] = {
    { ZEND_IS_EQUAL, NULL },
    { ZEND_IS_NOT_EQUAL, NULL },
    { ZEND_IS_SMALLER, NULL },
    { ZEND_IS_SMALLER_OR_EQUAL, NULL },
    { ZEND_SPACESHIP, NULL },
    { ZEND_CASE, NULL }
};

static int p748_cmps_handlers_active = 0;

static void p748_cmps_release_free_ops(zend_free_op free_op1, zend_free_op free_op2)
{
    if (free_op1) {
        zval_ptr_dtor_nogc(free_op1);
    }
    if (free_op2) {
        zval_ptr_dtor_nogc(free_op2);
    }
}

const char *p748_cmps_opcode_to_operator(zend_uchar opcode)
{
    switch (opcode) {
        case ZEND_IS_EQUAL:
            return "==";
        case ZEND_IS_NOT_EQUAL:
            return "!=";
        case ZEND_IS_SMALLER:
            return "<";
        case ZEND_IS_SMALLER_OR_EQUAL:
            return "<=";
        case ZEND_SPACESHIP:
            return "<=>";
        case ZEND_CASE:
            return "case";
        default:
            return "comparison";
    }
}

/* Why: handlers are global; enable/disable only once per process. */
static void p748_cmps_enable_handlers(void)
{
    size_t index;

    if (p748_cmps_handlers_active) {
        return;
    }

    for (index = 0; index < sizeof(p748_cmps_opcode_handlers)
        / sizeof(p748_cmps_opcode_handlers[0]); index++) {
        p748_cmps_opcode_handlers[index].previous =
            zend_get_user_opcode_handler(p748_cmps_opcode_handlers[index].opcode);
        zend_set_user_opcode_handler(
            p748_cmps_opcode_handlers[index].opcode,
            p748_cmps_opcode_handler);
    }

    p748_cmps_handlers_active = 1;
}

void p748_cmps_disable_handlers(void)
{
    size_t index;

    if (!p748_cmps_handlers_active) {
        return;
    }

    for (index = 0; index < sizeof(p748_cmps_opcode_handlers)
        / sizeof(p748_cmps_opcode_handlers[0]); index++) {
        zend_set_user_opcode_handler(
            p748_cmps_opcode_handlers[index].opcode,
            p748_cmps_opcode_handlers[index].previous);
    }

    p748_cmps_handlers_active = 0;
}

void p748_cmps_apply_mode(void)
{
    if (PHP74_PHP8_CS_G(mode) == P748_CMPS_MODE_OFF) {
        p748_cmps_disable_handlers();
        return;
    }

    p748_cmps_enable_handlers();
}

static int p748_cmps_opcode_handler(zend_execute_data *execute_data)
{
    const zend_op *opline = execute_data->opline;
    zend_free_op free_op1 = NULL;
    zend_free_op free_op2 = NULL;
    zval *op1;
    zval *op2;
    zend_long mode;
    int should_report = 0;
    int opcode_result = ZEND_USER_OPCODE_DISPATCH;
    int advance_opline = 0;

    mode = PHP74_PHP8_CS_G(mode);

    if (mode == P748_CMPS_MODE_OFF) {
        return ZEND_USER_OPCODE_DISPATCH;
    }

    op1 = zend_get_zval_ptr(opline, opline->op1_type, &opline->op1,
        execute_data, &free_op1, BP_VAR_R);
    op2 = zend_get_zval_ptr(opline, opline->op2_type, &opline->op2,
        execute_data, &free_op2, BP_VAR_R);

    if (op1 != NULL && op2 != NULL) {
        ZVAL_DEREF(op1);
        ZVAL_DEREF(op2);

        if (p748_cmps_is_number_string_pair(op1, op2)
            && p748_cmps_mode_uses_sampling(mode)) {
            zend_long factor = PHP74_PHP8_CS_G(sampling_factor);
            if (factor > 1) {
                PHP74_PHP8_CS_G(sample_counter)++;
                if ((PHP74_PHP8_CS_G(sample_counter) % factor) != 0) {
                    goto cleanup;
                }
            }
        }

        should_report = p748_cmps_should_report(op1, op2);

        if (should_report) {
            const char *op = p748_cmps_opcode_to_operator(opline->opcode);

            if (mode == P748_CMPS_MODE_ERROR) {
                zend_string *op1_str = zval_get_string(op1);
                zend_string *op2_str = zval_get_string(op2);

                zend_throw_error(NULL,
                    "php74_php8_comparison_shim: Non-strict comparison between "
                    "\"%s\" and \"%s\" using %s",
                    ZSTR_VAL(op1_str),
                    ZSTR_VAL(op2_str),
                    op);

                zend_string_release(op1_str);
                zend_string_release(op2_str);

                p748_cmps_release_free_ops(free_op1, free_op2);

                return ZEND_USER_OPCODE_CONTINUE;
            }

            if (p748_cmps_mode_reports(mode)) {
                if (p748_cmps_report_mode_defer(PHP74_PHP8_CS_G(report_mode))) {
                    p748_cmps_report_enqueue(opline->opcode, op1, op2);
                } else {
                    zend_string *op1_str = zval_get_string(op1);
                    zend_string *op2_str = zval_get_string(op2);

                    zend_error(E_DEPRECATED,
                        "php74_php8_comparison_shim: Non-strict comparison between "
                        "\"%s\" and \"%s\" using %s",
                        ZSTR_VAL(op1_str),
                        ZSTR_VAL(op2_str),
                        op);

                    zend_string_release(op1_str);
                    zend_string_release(op2_str);
                }
            }
        }

        if (should_report && p748_cmps_mode_simulates(mode)) {
            if (p748_cmps_simulate_php8_result(execute_data, opline, op1, op2)) {
                opcode_result = ZEND_USER_OPCODE_CONTINUE;
                advance_opline = 1;
            }
        }
    }

cleanup:
    /* Why: Only free operands when WE handle the opcode (CONTINUE). When we
     * return DISPATCH, the VM re-executes the standard handler which fetches
     * and frees the same TMP/VAR operands. Freeing here + DISPATCH = double-free
     * -> zend_mm_heap corrupted. */
    if (opcode_result != ZEND_USER_OPCODE_DISPATCH) {
        p748_cmps_release_free_ops(free_op1, free_op2);
    }

    if (advance_opline) {
        /*
         * Why: When we simulate, we already computed the result and return
         * ZEND_USER_OPCODE_CONTINUE to skip the original opcode handler. In
         * some VM dispatch variants, CONTINUE does not advance the instruction
         * pointer automatically; it keeps using execute_data->opline as-is.
         * If we leave opline unchanged, the VM will re-enter the same opcode
         * and repeat this handler indefinitely (busy loop / hang). Advancing
         * to opline + 1 makes execution proceed exactly as if the original
         * opcode had run and produced the result.
         */
        execute_data->opline = opline + 1;
    }

    return opcode_result;
}
