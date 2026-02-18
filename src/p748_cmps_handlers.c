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

/* Why: Store zend_string* only, not zvals. Storing zvals (ZVAL_COPY) keeps
 * references to refcounted request data; if PHP tears down the request before
 * or during our RSHUTDOWN, flush/dtor touch freed memory → zend_mm_heap corrupted.
 * We take the string form in the opcode handler (when operands are valid) and
 * own those refs until dtor. */
typedef struct {
    zend_string *op1_str;
    zend_string *op2_str;
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

    if (entry->op1_str != NULL) {
        zend_string_release(entry->op1_str);
        entry->op1_str = NULL;
    }
    if (entry->op2_str != NULL) {
        zend_string_release(entry->op2_str);
        entry->op2_str = NULL;
    }
    if (entry->filename != NULL) {
        efree((void *)entry->filename);
        entry->filename = NULL;
    }

    efree(entry);
}

static void p748_cmps_report_emit_entry(const p748_cmps_report_entry *entry)
{
    const char *op = p748_cmps_opcode_to_operator(entry->opcode);
    const char *filename = entry->filename ? entry->filename : "Unknown";
    const char *s1 = entry->op1_str ? ZSTR_VAL(entry->op1_str) : "?";
    const char *s2 = entry->op2_str ? ZSTR_VAL(entry->op2_str) : "?";

    if (entry->count > 1) {
        zend_error(E_DEPRECATED,
            "php74_php8_comparison_shim: Non-strict comparison between "
            "\"%s\" and \"%s\" using %s (repeated %ld times) in %s on line %ld",
            s1, s2, op, entry->count, filename, entry->lineno);
    } else {
        zend_error(E_DEPRECATED,
            "php74_php8_comparison_shim: Non-strict comparison between "
            "\"%s\" and \"%s\" using %s in %s on line %ld",
            s1, s2, op, filename, entry->lineno);
    }
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
    HashTable *ht;

    PHP74_PHP8_CS_G(report_overflowed) = 0;

    /* Why: Defensive check - if already initialized, clean up first to prevent
     * double-initialization which could leak memory or corrupt state. */
    if (PHP74_PHP8_CS_G(report_table_init)) {
        ht = &PHP74_PHP8_CS_G(report_table);
        if (ht->nTableMask != 0 && ht->nTableMask != (uint32_t)-1) {
            zend_hash_destroy(ht);
        }
    }

    PHP74_PHP8_CS_G(report_table_init) = 0;

    if (!p748_cmps_report_mode_defer(PHP74_PHP8_CS_G(report_mode))) {
        return;
    }
    if (!p748_cmps_mode_reports(PHP74_PHP8_CS_G(mode))) {
        return;
    }

    /* Why: Ensure a clean struct before init (e.g. after previous request's
     * shutdown or if shutdown was skipped). Avoids reusing stale pointers. */
    memset(&PHP74_PHP8_CS_G(report_table), 0, sizeof(HashTable));
    zend_hash_init(&PHP74_PHP8_CS_G(report_table), 8, NULL, p748_cmps_report_entry_dtor, 0);
    PHP74_PHP8_CS_G(report_table_init) = 1;
}

void p748_cmps_report_buffer_flush(void)
{
    p748_cmps_report_entry *entry;
    HashTable *ht;

    if (!PHP74_PHP8_CS_G(report_table_init)) {
        return;
    }

    ht = &PHP74_PHP8_CS_G(report_table);

    /* Why: Defensive check to prevent crash if HashTable is in invalid state.
     * Check nTableMask for obvious corruption indicators (0 = never initialized,
     * 0xFFFFFFFF = likely corrupted). */
    if (ht->nTableMask == 0 || ht->nTableMask == (uint32_t)-1) {
        php_error_docref(NULL, E_WARNING,
            "php74_php8_comparison_shim: HashTable in invalid state during flush, "
            "skipping to prevent crash");
        PHP74_PHP8_CS_G(report_table_init) = 0;
        return;
    }

    ZEND_HASH_FOREACH_PTR(ht, entry) {
        p748_cmps_report_emit_entry(entry);
    } ZEND_HASH_FOREACH_END();

    if (PHP74_PHP8_CS_G(report_overflowed)) {
        php_error_docref(NULL, E_WARNING,
            "php74_php8_comparison_shim.report_mode=defer: report buffer full, dropping further reports");
    }
}

void p748_cmps_report_buffer_shutdown(void)
{
    HashTable *ht;

    if (!PHP74_PHP8_CS_G(report_table_init)) {
        return;
    }

    ht = &PHP74_PHP8_CS_G(report_table);

    /* Why: Defensive check to prevent crash during cleanup. If HashTable is in
     * an invalid state (corrupted or never properly initialized despite flag),
     * skip destruction to avoid SIGSEGV. Better to leak memory than crash FPM. */
    if (ht->nTableMask == 0 || ht->nTableMask == (uint32_t)-1) {
        php_error_docref(NULL, E_WARNING,
            "php74_php8_comparison_shim: HashTable in invalid state during shutdown, "
            "skipping cleanup to prevent crash");
        PHP74_PHP8_CS_G(report_table_init) = 0;
        return;
    }

    zend_hash_destroy(ht);
    /* Why: Zero the struct so the next request's RINIT never reuses stale
     * HashTable internals (arData, etc.). Prevents zend_mm_heap corrupted
     * on the second request after a heavy first request in FPM. */
    memset(ht, 0, sizeof(HashTable));
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

    /* Why: Defensive validation before accessing HashTable operations.
     * If table is corrupted, disable the feature rather than crash. */
    if (table->nTableMask == 0 || table->nTableMask == (uint32_t)-1) {
        php_error_docref(NULL, E_WARNING,
            "php74_php8_comparison_shim: HashTable corrupted, disabling deferred reporting");
        PHP74_PHP8_CS_G(report_table_init) = 0;
        return;
    }
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

    /* Why: Store string form only (we take ownership). Do not store zvals:
     * they reference request-lifecycle data that may be freed before RSHUTDOWN,
     * causing use-after-free / double-free and zend_mm_heap corrupted. */
    entry = (p748_cmps_report_entry *)emalloc(sizeof(*entry));
    entry->op1_str = zval_get_string(op1);
    entry->op2_str = zval_get_string(op2);
    entry->opcode = opcode;
    entry->filename = (filename_cstr != NULL && filename_len > 0)
        ? (const char *)estrndup(filename_cstr, filename_len)
        : NULL;
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
     * → zend_mm_heap corrupted. */
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
