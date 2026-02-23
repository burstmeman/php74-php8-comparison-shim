#include "p748_cmps_internal.h"

#include "php_ini.h"
#include <string.h>

/* --- Ignored locations (path_suffix:line) --- */

void p748_cmps_ignored_locations_init(void)
{
    HashTable *ht = &PHP74_PHP8_CS_G(ignored_locations);

    if (PHP74_PHP8_CS_G(ignored_locations_init) && ht->nTableMask) {
        zend_hash_destroy(ht);
    }
    memset(ht, 0, sizeof(HashTable));
    zend_hash_init(ht, 8, NULL, (dtor_func_t)zval_ptr_dtor, 0);
    PHP74_PHP8_CS_G(ignored_locations_init) = 1;
}

void p748_cmps_ignored_locations_shutdown(void)
{
    HashTable *ht = &PHP74_PHP8_CS_G(ignored_locations);

    if (!PHP74_PHP8_CS_G(ignored_locations_init)) {
        return;
    }
    if (ht->nTableMask) {
        zend_hash_destroy(ht);
    }
    memset(ht, 0, sizeof(HashTable));
    PHP74_PHP8_CS_G(ignored_locations_init) = 0;
}

/* Pattern "path:line" - path is suffix of full path. Split on last ':'. */
int p748_cmps_location_is_ignored(const char *filename, zend_long lineno)
{
    HashTable *ht = &PHP74_PHP8_CS_G(ignored_locations);
    zval *val;
    const char *p, *path;
    size_t path_len, full_len;
    zend_long line;

    if (!PHP74_PHP8_CS_G(ignored_locations_init) || !ht->nTableMask) {
        return 0;
    }
    if (zend_hash_num_elements(ht) == 0) {
        return 0;
    }

    filename = filename ? filename : "";
    full_len = strlen(filename);

    ZEND_HASH_FOREACH_VAL(ht, val) {
        if (Z_TYPE_P(val) != IS_STRING) {
            continue;
        }
        p = ZSTR_VAL(Z_STR_P(val));
        path_len = ZSTR_LEN(Z_STR_P(val));

        /* Find last ':' for Windows path compatibility */
        while (path_len > 0 && p[path_len - 1] != ':') {
            path_len--;
        }
        if (path_len == 0) {
            continue;
        }
        path = p;
        path_len--;
        line = zend_atol(p + path_len + 1, ZSTR_LEN(Z_STR_P(val)) - path_len - 1);

        if (line >= 0 && line != lineno) {
            continue;
        }
        if (full_len >= path_len && memcmp(filename + full_len - path_len, path, path_len) == 0) {
            return 1;
        }
    } ZEND_HASH_FOREACH_END();

    return 0;
}

static void p748_cmps_set_ignored_locations_from_array(zval *array)
{
    HashTable *ht = &PHP74_PHP8_CS_G(ignored_locations);
    zval *val;
    zval zv;

    if (!array || Z_TYPE_P(array) != IS_ARRAY) {
        return;
    }
    if (!PHP74_PHP8_CS_G(ignored_locations_init)) {
        p748_cmps_ignored_locations_init();
    }
    if (!ht->nTableMask) {
        return;
    }

    zend_hash_clean(ht);
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(array), val) {
        if (Z_TYPE_P(val) == IS_STRING && Z_STRLEN_P(val) > 0) {
            ZVAL_STR(&zv, zend_string_copy(Z_STR_P(val)));
            zend_hash_next_index_insert(ht, &zv);
        }
    } ZEND_HASH_FOREACH_END();
}

/* --- PHP API --- */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_php74_php8_cmps_set_sampling, 0, 1, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, sampling_factor, IS_LONG, 0)
ZEND_END_ARG_INFO()

PHP_FUNCTION(php74_php8_cmps_set_sampling)
{
    zend_long factor;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(factor)
    ZEND_PARSE_PARAMETERS_END();

    if (factor < 0) {
        factor = 0;
    }
    if (p748_cmps_mode_forces_sampling_off(PHP74_PHP8_CS_G(mode))) {
        p748_cmps_disable_sampling();
        RETURN_FALSE;
    }
    PHP74_PHP8_CS_G(sampling_factor) = factor;
    PHP74_PHP8_CS_G(sample_counter) = 0;
    RETURN_TRUE;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_php74_php8_cmps_flush_deferred, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

PHP_FUNCTION(php74_php8_cmps_flush_deferred)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_BOOL(p748_cmps_report_buffer_flush());
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_php74_php8_cmps_set_ignored_locations, 0, 1, IS_VOID, 0)
    ZEND_ARG_TYPE_INFO(0, locations, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

PHP_FUNCTION(php74_php8_cmps_set_ignored_locations)
{
    zval *locations;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_ARRAY(locations)
    ZEND_PARSE_PARAMETERS_END();

    p748_cmps_set_ignored_locations_from_array(locations);
}

const zend_function_entry php74_php8_comparison_shim_functions[] = {
    PHP_FE(php74_php8_cmps_set_sampling, arginfo_php74_php8_cmps_set_sampling)
    PHP_FE(php74_php8_cmps_flush_deferred, arginfo_php74_php8_cmps_flush_deferred)
    PHP_FE(php74_php8_cmps_set_ignored_locations, arginfo_php74_php8_cmps_set_ignored_locations)
    PHP_FE_END
};
