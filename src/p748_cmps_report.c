#include "p748_cmps_internal.h"

#include "Zend/zend_smart_str.h"

#include <string.h>

/* Why: Store zend_string* only, not zvals. Storing zvals (ZVAL_COPY) keeps
 * references to refcounted request data; if PHP tears down the request before
 * or during our RSHUTDOWN, flush/dtor touch freed memory -> zend_mm_heap corrupted.
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

    zend_error_at(E_DEPRECATED, filename, (uint32_t)entry->lineno,
        "php74_php8_comparison_shim: Non-strict comparison between "
        "\"%s\" and \"%s\" using %s (repeated %ld times)",
        s1, s2, op, entry->count);
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
    PHP74_PHP8_CS_G(report_flushed) = 0;

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

zend_bool p748_cmps_report_buffer_flush(void)
{
    p748_cmps_report_entry *entry;
    HashTable *ht;
    zend_bool had_entries;

    if (PHP74_PHP8_CS_G(report_flushed)) {
        return 0;
    }

    if (!PHP74_PHP8_CS_G(report_table_init)) {
        return 0;
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
        PHP74_PHP8_CS_G(report_flushed) = 1;
        return 0;
    }

    had_entries = zend_hash_num_elements(ht) > 0;

    /* Why: Prevent reentrancy during flush. User error handlers triggered by
     * zend_error() might perform comparisons, which would enter the opcode
     * handler and try to enqueue new entries while we iterate the HashTable. */
    PHP74_PHP8_CS_G(in_handler) = 1;

    ZEND_HASH_FOREACH_PTR(ht, entry) {
        p748_cmps_report_emit_entry(entry);
    } ZEND_HASH_FOREACH_END();

    if (PHP74_PHP8_CS_G(report_overflowed)) {
        php_error_docref(NULL, E_WARNING,
            "php74_php8_comparison_shim.report_mode=defer: report buffer full, dropping further reports");
    }

    PHP74_PHP8_CS_G(in_handler) = 0;
    PHP74_PHP8_CS_G(report_flushed) = 1;

    return had_entries;
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

void p748_cmps_report_get_deferred(zval *return_value)
{
    p748_cmps_report_entry *entry;
    HashTable *ht;
    zval row;
    const char *filename;
    const char *op;
    const char *s1;
    const char *s2;

    array_init(return_value);

    if (!p748_cmps_report_mode_defer(PHP74_PHP8_CS_G(report_mode))) {
        return;
    }
    if (!PHP74_PHP8_CS_G(report_table_init)) {
        return;
    }

    ht = &PHP74_PHP8_CS_G(report_table);
    if (ht->nTableMask == 0 || ht->nTableMask == (uint32_t)-1) {
        return;
    }

    ZEND_HASH_FOREACH_PTR(ht, entry) {
        array_init(&row);
        filename = entry->filename ? entry->filename : "Unknown";
        op = p748_cmps_opcode_to_operator(entry->opcode);
        s1 = entry->op1_str ? ZSTR_VAL(entry->op1_str) : "?";
        s2 = entry->op2_str ? ZSTR_VAL(entry->op2_str) : "?";

        add_assoc_string(&row, "filename", filename);
        add_assoc_long(&row, "line", entry->lineno);
        add_assoc_long(&row, "entry_count", entry->count);
        add_assoc_string(&row, "operator", op);
        add_assoc_string(&row, "left_op", s1);
        add_assoc_string(&row, "right_op", s2);

        add_next_index_zval(return_value, &row);
    } ZEND_HASH_FOREACH_END();
}
