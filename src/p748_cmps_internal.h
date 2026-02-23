#ifndef PHP74_PHP8_COMPARISON_SHIM_INTERNAL_H
#define PHP74_PHP8_COMPARISON_SHIM_INTERNAL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php74_php8_comparison_shim.h"
#include "zend_operators.h"

/* Module globals definition - internal use only */
ZEND_BEGIN_MODULE_GLOBALS(php74_php8_comparison_shim)
    zend_long mode;
    zend_long sampling_factor;
    zend_long sample_counter;
    zend_long report_mode;
    zend_long report_limit;
    zend_bool report_overflowed;
    zend_bool report_table_init;
    zend_bool in_handler;
    zend_bool report_flushed;
    HashTable report_table;
    HashTable ignored_locations;
    zend_bool ignored_locations_init;
ZEND_END_MODULE_GLOBALS(php74_php8_comparison_shim)

ZEND_EXTERN_MODULE_GLOBALS(php74_php8_comparison_shim)

#define PHP74_PHP8_CS_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(php74_php8_comparison_shim, v)

#define P748_CMPS_MODE_OFF 0
#define P748_CMPS_MODE_REPORT 1
#define P748_CMPS_MODE_ERROR 2
#define P748_CMPS_MODE_SIMULATE_AND_REPORT 3
#define P748_CMPS_MODE_SIMULATE 4

#define P748_CMPS_REPORT_MODE_SYNC 0
#define P748_CMPS_REPORT_MODE_DEFER 1

/* Public surface for cross-file use. */
void p748_cmps_set_mode_from_string(const zend_string *value);
void p748_cmps_set_sampling_from_string(const zend_string *value);
void p748_cmps_set_report_mode_from_string(const zend_string *value);
void p748_cmps_set_mode_from_cstr(const char *value);
void p748_cmps_set_sampling_from_cstr(const char *value);
void p748_cmps_set_report_mode_from_cstr(const char *value);
void p748_cmps_apply_mode(void);
void p748_cmps_disable_handlers(void);
const char *p748_cmps_mode_to_string(zend_long mode);
const char *p748_cmps_report_mode_to_string(zend_long mode);
void p748_cmps_report_buffer_init(void);
zend_bool p748_cmps_report_buffer_flush(void);
void p748_cmps_report_buffer_shutdown(void);
void p748_cmps_report_enqueue(zend_uchar opcode, zval *op1, zval *op2);
void p748_cmps_ignored_locations_init(void);
void p748_cmps_ignored_locations_shutdown(void);
int p748_cmps_location_is_ignored(const char *filename, zend_long lineno);

extern const zend_function_entry php74_php8_comparison_shim_functions[];
const char *p748_cmps_opcode_to_operator(zend_uchar opcode);
int p748_cmps_simulate_php8_result(zend_execute_data *execute_data,
    const zend_op *opline, zval *op1, zval *op2);

/* Inline helpers used by multiple compilation units.
 * Note: Using 'static' instead of 'static inline' for C89 compatibility.
 * Modern compilers will inline these automatically when optimization is enabled. */

#if defined(__GNUC__) || defined(__clang__)
#define P748_INLINE static __inline__
#elif defined(_MSC_VER)
#define P748_INLINE static __inline
#else
#define P748_INLINE static
#endif

P748_INLINE int p748_cmps_mode_forces_sampling_off(zend_long mode)
{
    return mode == P748_CMPS_MODE_ERROR
        || mode == P748_CMPS_MODE_SIMULATE_AND_REPORT
        || mode == P748_CMPS_MODE_SIMULATE;
}

P748_INLINE int p748_cmps_mode_uses_sampling(zend_long mode)
{
    return !p748_cmps_mode_forces_sampling_off(mode)
        && mode != P748_CMPS_MODE_OFF;
}

P748_INLINE int p748_cmps_mode_reports(zend_long mode)
{
    return mode == P748_CMPS_MODE_REPORT
        || mode == P748_CMPS_MODE_SIMULATE_AND_REPORT;
}

P748_INLINE int p748_cmps_mode_simulates(zend_long mode)
{
    return mode == P748_CMPS_MODE_SIMULATE
        || mode == P748_CMPS_MODE_SIMULATE_AND_REPORT;
}

P748_INLINE int p748_cmps_report_mode_defer(zend_long mode)
{
    return mode == P748_CMPS_REPORT_MODE_DEFER;
}

P748_INLINE void p748_cmps_disable_sampling(void)
{
    PHP74_PHP8_CS_G(sampling_factor) = 0;
    PHP74_PHP8_CS_G(sample_counter) = 0;
}

P748_INLINE int p748_cmps_is_number(const zval *value)
{
    return Z_TYPE_P(value) == IS_LONG || Z_TYPE_P(value) == IS_DOUBLE;
}

P748_INLINE int p748_cmps_is_strict_numeric_string(const zend_string *value)
{
    zend_long lval;
    double dval;

    return is_numeric_string(ZSTR_VAL(value), ZSTR_LEN(value), &lval, &dval, 0) != 0;
}

P748_INLINE int p748_cmps_is_non_numeric_string(const zval *value)
{
    if (Z_TYPE_P(value) != IS_STRING) {
        return 0;
    }

    return p748_cmps_is_strict_numeric_string(Z_STR_P(value)) == 0;
}

P748_INLINE int p748_cmps_should_report(const zval *op1, const zval *op2)
{
    /* Why: only number vs non-numeric string changed in PHP 8.0. */
    return (p748_cmps_is_number(op1) && p748_cmps_is_non_numeric_string(op2))
        || (p748_cmps_is_number(op2) && p748_cmps_is_non_numeric_string(op1));
}

P748_INLINE int p748_cmps_is_number_string_pair(const zval *op1, const zval *op2)
{
    return (p748_cmps_is_number(op1) && Z_TYPE_P(op2) == IS_STRING)
        || (p748_cmps_is_number(op2) && Z_TYPE_P(op1) == IS_STRING);
}

#endif /* PHP74_PHP8_COMPARISON_SHIM_INTERNAL_H */
