#include "p748_cmps_internal.h"

#include "zend_execute.h"
#include "zend_exceptions.h"
#include "Zend/zend_smart_str.h"

#include <string.h>

typedef struct {
    zend_uchar opcode;
    user_opcode_handler_t previous;
} p748_cmps_opcode_handler_entry;

static int p748_cmps_opcode_handler(zend_execute_data *execute_data);
static const char *p748_cmps_opcode_to_operator(zend_uchar opcode);

typedef struct {
    zval op1;
    zval op2;
    zend_uchar opcode;
    const char *filename;
    zend_long lineno;
    zend_long count;
} p748_cmps_report_entry;

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

static void p748_cmps_report_entry_dtor(zval *zv)
{
    p748_cmps_report_entry *entry = (p748_cmps_report_entry *)Z_PTR_P(zv);

    if (entry == NULL) {
        return;
    }

    zval_ptr_dtor(&entry->op1);
    zval_ptr_dtor(&entry->op2);
    efree(entry);
}

static void p748_cmps_report_emit_entry(const p748_cmps_report_entry *entry)
{
    const char *op = p748_cmps_opcode_to_operator(entry->opcode);
    const char *filename = entry->filename ? entry->filename : "Unknown";
    zend_string *op1_str = zval_get_string((zval *)&entry->op1);
    zend_string *op2_str = zval_get_string((zval *)&entry->op2);

    if (entry->count > 1) {
        zend_error(E_DEPRECATED,
            "php74_php8_comparison_shim.mode: Non-strict comparison between "
            "\"%s\" and \"%s\" using %s (repeated %ld times) in %s on line %ld",
            ZSTR_VAL(op1_str),
            ZSTR_VAL(op2_str),
            op,
            entry->count,
            filename,
            entry->lineno);
    } else {
        zend_error(E_DEPRECATED,
            "php74_php8_comparison_shim.mode: Non-strict comparison between "
            "\"%s\" and \"%s\" using %s in %s on line %ld",
            ZSTR_VAL(op1_str),
            ZSTR_VAL(op2_str),
            op,
            filename,
            entry->lineno);
    }

    zend_string_release(op1_str);
    zend_string_release(op2_str);
}

static void p748_cmps_report_key_init(smart_str *key, const char *filename, size_t filename_len, zend_long lineno)
{
    if (filename != NULL && filename_len > 0) {
        smart_str_append_long(key, filename_len);
        smart_str_appendc(key, ':');
        smart_str_appendl(key, filename, filename_len);
    } else {
        smart_str_append_long(key, 0);
        smart_str_appendc(key, ':');
    }
    smart_str_appendc(key, '|');
    smart_str_append_long(key, lineno);
    smart_str_0(key);
}

void p748_cmps_report_buffer_init(void)
{
    PHP74_PHP8_CS_G(report_overflowed) = 0;
    PHP74_PHP8_CS_G(report_table_init) = 0;

    if (!p748_cmps_report_mode_defer(PHP74_PHP8_CS_G(report_mode))) {
        return;
    }
    if (!p748_cmps_mode_reports(PHP74_PHP8_CS_G(mode))) {
        return;
    }

    zend_hash_init(&PHP74_PHP8_CS_G(report_table), 8, NULL, p748_cmps_report_entry_dtor, 0);
    PHP74_PHP8_CS_G(report_table_init) = 1;
}

void p748_cmps_report_buffer_flush(void)
{
    p748_cmps_report_entry *entry;

    if (!PHP74_PHP8_CS_G(report_table_init)) {
        return;
    }

    ZEND_HASH_FOREACH_PTR(&PHP74_PHP8_CS_G(report_table), entry) {
        p748_cmps_report_emit_entry(entry);
    } ZEND_HASH_FOREACH_END();

    if (PHP74_PHP8_CS_G(report_overflowed)) {
        zend_error(E_WARNING,
            "php74_php8_comparison_shim.report_mode=defer: report buffer full, dropping further reports");
    }
}

void p748_cmps_report_buffer_shutdown(void)
{
    if (!PHP74_PHP8_CS_G(report_table_init)) {
        return;
    }

    zend_hash_destroy(&PHP74_PHP8_CS_G(report_table));
    PHP74_PHP8_CS_G(report_table_init) = 0;
}

void p748_cmps_report_enqueue(zend_uchar opcode, zval *op1, zval *op2)
{
    p748_cmps_report_entry *entry;
    HashTable *table;
    smart_str key = {0};
    zend_string *key_str;
    const char *filename_cstr;
    size_t filename_len;
    zend_long lineno;

    if (!PHP74_PHP8_CS_G(report_table_init)) {
        return;
    }

    table = &PHP74_PHP8_CS_G(report_table);
    filename_cstr = zend_get_executed_filename();
    lineno = zend_get_executed_lineno();
    filename_len = filename_cstr != NULL ? strlen(filename_cstr) : 0;

    /* Key by filename + line so we only report once per file:line. */
    p748_cmps_report_key_init(&key, filename_cstr, filename_len, lineno);

    key_str = key.s;
    if (key_str == NULL) {
        return;
    }

    entry = zend_hash_find_ptr(table, key_str);
    if (entry != NULL) {
        entry->count++;
        zend_string_release(key_str);
        return;
    }

    if (PHP74_PHP8_CS_G(report_limit) > 0
        && zend_hash_num_elements(table) >= (uint32_t)PHP74_PHP8_CS_G(report_limit)) {
        PHP74_PHP8_CS_G(report_overflowed) = 1;
        zend_string_release(key_str);
        return;
    }

    entry = (p748_cmps_report_entry *)emalloc(sizeof(*entry));
    ZVAL_COPY(&entry->op1, op1);
    ZVAL_COPY(&entry->op2, op2);
    entry->opcode = opcode;
    entry->filename = filename_cstr;
    entry->lineno = lineno;
    entry->count = 1;

    zend_hash_add_ptr(table, key_str, entry);
    zend_string_release(key_str);
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

static const char *p748_cmps_opcode_to_operator(zend_uchar opcode)
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

static int p748_cmps_simulate_php8_result(
    zend_execute_data *execute_data,
    const zend_op *opline,
    zval *op1,
    zval *op2)
{
    zend_string *op1_str;
    zend_string *op2_str;
    int cmp;
    zval *result;

    if (!p748_cmps_should_report(op1, op2)) {
        return 0;
    }

    /* Why: PHP 8 compares non-numeric string vs number as strings. */
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
                    "php74_php8_comparison_shim.mode: Non-strict comparison between "
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
                        "php74_php8_comparison_shim.mode: Non-strict comparison between "
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
    p748_cmps_release_free_ops(free_op1, free_op2);

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
