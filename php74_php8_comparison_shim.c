#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "src/p748_cmps_internal.h"

#include "php_ini.h"
#include "ext/standard/info.h"
#include <string.h>

ZEND_DECLARE_MODULE_GLOBALS(php74_php8_comparison_shim)

static ZEND_INI_MH(p748_cmps_update_mode)
{
	if (stage == PHP_INI_STAGE_RUNTIME || stage == PHP_INI_STAGE_HTACCESS) {
		return FAILURE;
	}

	/* Why: mode changes affect opcode handlers and must be set only at startup. */
	p748_cmps_set_mode_from_string(new_value);
	return SUCCESS;
}

static ZEND_INI_MH(p748_cmps_update_sampling_factor)
{
	if (stage == PHP_INI_STAGE_RUNTIME || stage == PHP_INI_STAGE_HTACCESS) {
		return FAILURE;
	}

	/* Why: sampling affects reporting frequency and is fixed for process lifetime. */
	p748_cmps_set_sampling_from_string(new_value);
	return SUCCESS;
}

static ZEND_INI_MH(p748_cmps_update_report_mode)
{
	if (stage == PHP_INI_STAGE_RUNTIME || stage == PHP_INI_STAGE_HTACCESS) {
		return FAILURE;
	}

	/* Why: report mode is fixed for process lifetime. */
	p748_cmps_set_report_mode_from_string(new_value);
	return SUCCESS;
}

static ZEND_INI_MH(p748_cmps_update_report_limit)
{
	zend_long limit = 0;

	if (stage == PHP_INI_STAGE_RUNTIME || stage == PHP_INI_STAGE_HTACCESS) {
		return FAILURE;
	}

	if (new_value != NULL && ZSTR_LEN(new_value) > 0) {
		limit = zend_atol(ZSTR_VAL(new_value), ZSTR_LEN(new_value));
	}

	if (limit < 0) {
		limit = 0;
	}

	PHP74_PHP8_CS_G(report_limit) = limit;
	return SUCCESS;
}

PHP_INI_BEGIN()
	PHP_INI_ENTRY("php74_php8_comparison_shim.mode", "Off", PHP_INI_SYSTEM, p748_cmps_update_mode)
	PHP_INI_ENTRY("php74_php8_comparison_shim.sampling_factor", "0", PHP_INI_SYSTEM, p748_cmps_update_sampling_factor)
	PHP_INI_ENTRY("php74_php8_comparison_shim.report_mode", "sync", PHP_INI_SYSTEM, p748_cmps_update_report_mode)
	PHP_INI_ENTRY("php74_php8_comparison_shim.report_limit", "128", PHP_INI_SYSTEM, p748_cmps_update_report_limit)
PHP_INI_END()

static void p748_cmps_init_globals(void *globals)
{
	zend_php74_php8_comparison_shim_globals *cmps_globals =
		(zend_php74_php8_comparison_shim_globals *)globals;

	cmps_globals->mode = P748_CMPS_MODE_OFF;
	cmps_globals->sampling_factor = 0;
	cmps_globals->sample_counter = 0;
	cmps_globals->report_mode = P748_CMPS_REPORT_MODE_SYNC;
	cmps_globals->report_limit = 128;
	cmps_globals->report_overflowed = 0;
	cmps_globals->report_table_init = 0;
}

static PHP_MINIT_FUNCTION(php74_php8_comparison_shim)
{
	REGISTER_INI_ENTRIES();
	p748_cmps_set_mode_from_cstr(INI_STR("php74_php8_comparison_shim.mode"));
	p748_cmps_set_sampling_from_cstr(INI_STR("php74_php8_comparison_shim.sampling_factor"));
	p748_cmps_set_report_mode_from_cstr(INI_STR("php74_php8_comparison_shim.report_mode"));
	{
		const char *limit_str = INI_STR("php74_php8_comparison_shim.report_limit");
		zend_long limit = 0;

		if (limit_str != NULL && limit_str[0] != '\0') {
			limit = zend_atol(limit_str, strlen(limit_str));
		}
		if (limit < 0) {
			limit = 0;
		}
		PHP74_PHP8_CS_G(report_limit) = limit;
	}
	p748_cmps_apply_mode();

	return SUCCESS;
}

static PHP_RINIT_FUNCTION(php74_php8_comparison_shim)
{
#if defined(ZTS) && defined(COMPILE_DL_PHP74_PHP8_COMPARISON_SHIM)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	p748_cmps_report_buffer_init();
	return SUCCESS;
}

static PHP_RSHUTDOWN_FUNCTION(php74_php8_comparison_shim)
{
	p748_cmps_report_buffer_flush();
	p748_cmps_report_buffer_shutdown();
	return SUCCESS;
}

static PHP_MSHUTDOWN_FUNCTION(php74_php8_comparison_shim)
{
	UNREGISTER_INI_ENTRIES();
	p748_cmps_disable_handlers();

	return SUCCESS;
}

static PHP_MINFO_FUNCTION(php74_php8_comparison_shim)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "php74_php8_comparison_shim support", "enabled");
	php_info_print_table_row(2, "Mode", p748_cmps_mode_to_string(PHP74_PHP8_CS_G(mode)));
	php_info_print_table_row(2, "Sampling factor", INI_STR("php74_php8_comparison_shim.sampling_factor"));
	php_info_print_table_row(2, "Report mode", p748_cmps_report_mode_to_string(PHP74_PHP8_CS_G(report_mode)));
	php_info_print_table_row(2, "Report limit", INI_STR("php74_php8_comparison_shim.report_limit"));
#if PHP74_PHP8_COMPARISON_SHIM_RISKY
	php_info_print_table_row(2, "Risky modes", "enabled");
#else
	php_info_print_table_row(2, "Risky modes", "disabled");
#endif
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}

zend_module_entry php74_php8_comparison_shim_module_entry = {
	STANDARD_MODULE_HEADER,
	"php74_php8_comparison_shim",
	NULL,
	PHP_MINIT(php74_php8_comparison_shim),
	PHP_MSHUTDOWN(php74_php8_comparison_shim),
	PHP_RINIT(php74_php8_comparison_shim),
	PHP_RSHUTDOWN(php74_php8_comparison_shim),
	PHP_MINFO(php74_php8_comparison_shim),
	PHP_PHP74_PHP8_COMPARISON_SHIM_VERSION,
	PHP_MODULE_GLOBALS(php74_php8_comparison_shim),
	p748_cmps_init_globals,
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_PHP74_PHP8_COMPARISON_SHIM
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(php74_php8_comparison_shim)
#endif
