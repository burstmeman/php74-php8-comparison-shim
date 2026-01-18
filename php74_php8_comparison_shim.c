#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "src/p748_cmps_internal.h"

#include "php_ini.h"
#include "ext/standard/info.h"

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

PHP_INI_BEGIN()
	PHP_INI_ENTRY("php74_php8_comparison_shim.mode", "Off", PHP_INI_SYSTEM, p748_cmps_update_mode)
	PHP_INI_ENTRY("php74_php8_comparison_shim.sampling_factor", "0", PHP_INI_SYSTEM, p748_cmps_update_sampling_factor)
PHP_INI_END()

static void p748_cmps_init_globals(void *globals)
{
	zend_php74_php8_comparison_shim_globals *cmps_globals =
		(zend_php74_php8_comparison_shim_globals *)globals;

	cmps_globals->mode = P748_CMPS_MODE_OFF;
	cmps_globals->sampling_factor = 0;
	cmps_globals->sample_counter = 0;
}

static PHP_MINIT_FUNCTION(php74_php8_comparison_shim)
{
	REGISTER_INI_ENTRIES();
	p748_cmps_set_mode_from_cstr(INI_STR("php74_php8_comparison_shim.mode"));
	p748_cmps_set_sampling_from_cstr(INI_STR("php74_php8_comparison_shim.sampling_factor"));
	p748_cmps_apply_mode();

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
	NULL,
	NULL,
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
