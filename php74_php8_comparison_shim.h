#ifndef PHP_PHP74_PHP8_COMPARISON_SHIM_H
#define PHP_PHP74_PHP8_COMPARISON_SHIM_H

#include "php.h"

extern zend_module_entry php74_php8_comparison_shim_module_entry;
#define phpext_php74_php8_comparison_shim_ptr &php74_php8_comparison_shim_module_entry

#define PHP_PHP74_PHP8_COMPARISON_SHIM_VERSION "0.1.0"

#ifdef PHP_WIN32
#define PHP_PHP74_PHP8_COMPARISON_SHIM_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#define PHP_PHP74_PHP8_COMPARISON_SHIM_API __attribute__ ((visibility("default")))
#else
#define PHP_PHP74_PHP8_COMPARISON_SHIM_API
#endif

ZEND_BEGIN_MODULE_GLOBALS(php74_php8_comparison_shim)
    zend_long mode;
    zend_long sampling_factor;
    zend_long sample_counter;
    zend_long report_mode;
    zend_long report_limit;
    zend_bool report_overflowed;
    zend_bool report_table_init;
    HashTable report_table;
ZEND_END_MODULE_GLOBALS(php74_php8_comparison_shim)

ZEND_EXTERN_MODULE_GLOBALS(php74_php8_comparison_shim)

#define PHP74_PHP8_CS_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(php74_php8_comparison_shim, v)

#endif /* PHP_PHP74_PHP8_COMPARISON_SHIM_H */
