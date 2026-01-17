#ifndef PHP_PHP80_STRING_NUMBER_COMPARISON_H
#define PHP_PHP80_STRING_NUMBER_COMPARISON_H

#include "php.h"

extern zend_module_entry php80_string_number_comparison_module_entry;
#define phpext_php80_string_number_comparison_ptr &php80_string_number_comparison_module_entry

#define PHP_PHP80_STRING_NUMBER_COMPARISON_VERSION "0.1.0"

#ifdef PHP_WIN32
#define PHP_PHP80_STRING_NUMBER_COMPARISON_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#define PHP_PHP80_STRING_NUMBER_COMPARISON_API __attribute__ ((visibility("default")))
#else
#define PHP_PHP80_STRING_NUMBER_COMPARISON_API
#endif

ZEND_BEGIN_MODULE_GLOBALS(php80_string_number_comparison)
	zend_long mode;
ZEND_END_MODULE_GLOBALS(php80_string_number_comparison)

ZEND_EXTERN_MODULE_GLOBALS(php80_string_number_comparison)

#define PHP80_SNC_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(php80_string_number_comparison, v)

#endif /* PHP_PHP80_STRING_NUMBER_COMPARISON_H */
