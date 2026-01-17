#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php74_php8_comparison_shim.h"

#include "php_ini.h"
#include "ext/standard/info.h"
#include "zend_execute.h"
#include "zend_exceptions.h"
#include "zend_operators.h"

#define PHP80_SNC_MODE_OFF 0
#define PHP80_SNC_MODE_REPORT 1
#define PHP80_SNC_MODE_ERROR 2
#define PHP80_SNC_MODE_SIMULATE_AND_REPORT 3
#define PHP80_SNC_MODE_SIMULATE 4

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

ZEND_DECLARE_MODULE_GLOBALS(php74_php8_comparison_shim)

static void p748_cmps_init_globals(zend_php74_php8_comparison_shim_globals *globals)
{
	globals->mode = PHP80_SNC_MODE_OFF;
	globals->sampling_factor = 0;
	globals->sample_counter = 0;
}

static int p748_cmps_handlers_active = 0;

static inline int p748_cmps_mode_forces_sampling_off(zend_long mode)
{
	return mode == PHP80_SNC_MODE_ERROR
		|| mode == PHP80_SNC_MODE_SIMULATE_AND_REPORT
		|| mode == PHP80_SNC_MODE_SIMULATE;
}

static inline int p748_cmps_mode_uses_sampling(zend_long mode)
{
	return !p748_cmps_mode_forces_sampling_off(mode)
		&& mode != PHP80_SNC_MODE_OFF;
}

static inline int p748_cmps_mode_reports(zend_long mode)
{
	return mode == PHP80_SNC_MODE_REPORT
		|| mode == PHP80_SNC_MODE_SIMULATE_AND_REPORT;
}

static inline int p748_cmps_mode_simulates(zend_long mode)
{
	return mode == PHP80_SNC_MODE_SIMULATE
		|| mode == PHP80_SNC_MODE_SIMULATE_AND_REPORT;
}

static inline void p748_cmps_disable_sampling(void)
{
	PHP74_PHP8_CS_G(sampling_factor) = 0;
	PHP74_PHP8_CS_G(sample_counter) = 0;
}

/* Install opcode handlers once when mode is enabled. */
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

/* Restore previous opcode handlers when mode is disabled. */
static void p748_cmps_disable_handlers(void)
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

/* Parse INI value (zend_string) into module mode. */
static void p748_cmps_set_mode_from_string(const zend_string *value)
{
	if (value == NULL || ZSTR_LEN(value) == 0) {
		PHP74_PHP8_CS_G(mode) = PHP80_SNC_MODE_OFF;
		return;
	}

	if (zend_string_equals_literal_ci(value, "off")
		|| zend_string_equals_literal_ci(value, "0")) {
		PHP74_PHP8_CS_G(mode) = PHP80_SNC_MODE_OFF;
		return;
	}

	if (zend_string_equals_literal_ci(value, "report")
		|| zend_string_equals_literal_ci(value, "1")) {
		PHP74_PHP8_CS_G(mode) = PHP80_SNC_MODE_REPORT;
		return;
	}

	if (zend_string_equals_literal_ci(value, "error")
		|| zend_string_equals_literal_ci(value, "2")) {
		PHP74_PHP8_CS_G(mode) = PHP80_SNC_MODE_ERROR;
		p748_cmps_disable_sampling();
		return;
	}

	if (zend_string_equals_literal_ci(value, "simulate_and_report")
		|| zend_string_equals_literal_ci(value, "3")) {
		PHP74_PHP8_CS_G(mode) = PHP80_SNC_MODE_SIMULATE_AND_REPORT;
		p748_cmps_disable_sampling();
		return;
	}

	if (zend_string_equals_literal_ci(value, "simulate")
		|| zend_string_equals_literal_ci(value, "4")) {
		PHP74_PHP8_CS_G(mode) = PHP80_SNC_MODE_SIMULATE;
		p748_cmps_disable_sampling();
		return;
	}

	PHP74_PHP8_CS_G(mode) = PHP80_SNC_MODE_OFF;
}

/* Parse INI value (zend_string) into sampling factor. */
static void p748_cmps_set_sampling_from_string(const zend_string *value)
{
	zend_long factor = 0;

	if (p748_cmps_mode_forces_sampling_off(PHP74_PHP8_CS_G(mode))) {
		p748_cmps_disable_sampling();
		return;
	}

	if (value != NULL && ZSTR_LEN(value) > 0) {
		factor = zend_atol(ZSTR_VAL(value), ZSTR_LEN(value));
	}

	if (factor < 0) {
		factor = 0;
	}

	PHP74_PHP8_CS_G(sampling_factor) = factor;
	PHP74_PHP8_CS_G(sample_counter) = 0;
}

/* Parse INI value (C string) into module mode. */
static void p748_cmps_set_mode_from_cstr(const char *value)
{
	if (value == NULL || value[0] == '\0') {
		PHP74_PHP8_CS_G(mode) = PHP80_SNC_MODE_OFF;
		return;
	}

	if (strcasecmp(value, "off") == 0 || strcmp(value, "0") == 0) {
		PHP74_PHP8_CS_G(mode) = PHP80_SNC_MODE_OFF;
		return;
	}

	if (strcasecmp(value, "report") == 0 || strcmp(value, "1") == 0) {
		PHP74_PHP8_CS_G(mode) = PHP80_SNC_MODE_REPORT;
		return;
	}

	if (strcasecmp(value, "error") == 0 || strcmp(value, "2") == 0) {
		PHP74_PHP8_CS_G(mode) = PHP80_SNC_MODE_ERROR;
		p748_cmps_disable_sampling();
		return;
	}

	if (strcasecmp(value, "simulate_and_report") == 0 || strcmp(value, "3") == 0) {
		PHP74_PHP8_CS_G(mode) = PHP80_SNC_MODE_SIMULATE_AND_REPORT;
		p748_cmps_disable_sampling();
		return;
	}

	if (strcasecmp(value, "simulate") == 0 || strcmp(value, "4") == 0) {
		PHP74_PHP8_CS_G(mode) = PHP80_SNC_MODE_SIMULATE;
		p748_cmps_disable_sampling();
		return;
	}

	PHP74_PHP8_CS_G(mode) = PHP80_SNC_MODE_OFF;
}

/* Parse INI value (C string) into sampling factor. */
static void p748_cmps_set_sampling_from_cstr(const char *value)
{
	zend_long factor = 0;

	if (p748_cmps_mode_forces_sampling_off(PHP74_PHP8_CS_G(mode))) {
		p748_cmps_disable_sampling();
		return;
	}

	if (value != NULL && value[0] != '\0') {
		factor = zend_atol(value, strlen(value));
	}

	if (factor < 0) {
		factor = 0;
	}

	PHP74_PHP8_CS_G(sampling_factor) = factor;
	PHP74_PHP8_CS_G(sample_counter) = 0;
}

/* Apply current mode by enabling or disabling opcode handlers. */
static void p748_cmps_apply_mode(void)
{
	if (PHP74_PHP8_CS_G(mode) == PHP80_SNC_MODE_OFF) {
		p748_cmps_disable_handlers();
		return;
	}

	p748_cmps_enable_handlers();
}

static const char *p748_cmps_mode_to_string(zend_long mode)
{
	switch (mode) {
		case PHP80_SNC_MODE_REPORT:
			return "report";
		case PHP80_SNC_MODE_ERROR:
			return "error";
		case PHP80_SNC_MODE_SIMULATE_AND_REPORT:
			return "simulate_and_report";
		case PHP80_SNC_MODE_SIMULATE:
			return "simulate";
		case PHP80_SNC_MODE_OFF:
		default:
			return "off";
	}
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

static inline int p748_cmps_is_number(const zval *value)
{
	return Z_TYPE_P(value) == IS_LONG || Z_TYPE_P(value) == IS_DOUBLE;
}

static inline int p748_cmps_is_strict_numeric_string(const zend_string *value)
{
	zend_long lval;
	double dval;

	return is_numeric_string(ZSTR_VAL(value), ZSTR_LEN(value), &lval, &dval, 0) != 0;
}

static inline int p748_cmps_is_non_numeric_string(const zval *value)
{
	if (Z_TYPE_P(value) != IS_STRING) {
		return 0;
	}

	return p748_cmps_is_strict_numeric_string(Z_STR_P(value)) == 0;
}

static inline int p748_cmps_should_report(const zval *op1, const zval *op2)
{
	/* Fast-path: report only when number vs non-numeric string. */
	return (p748_cmps_is_number(op1) && p748_cmps_is_non_numeric_string(op2))
		|| (p748_cmps_is_number(op2) && p748_cmps_is_non_numeric_string(op1));
}

static inline int p748_cmps_is_number_string_pair(const zval *op1, const zval *op2)
{
	return (p748_cmps_is_number(op1) && Z_TYPE_P(op2) == IS_STRING)
		|| (p748_cmps_is_number(op2) && Z_TYPE_P(op1) == IS_STRING);
}

static int p748_cmps_simulate_php8_result(
	zend_execute_data *execute_data,
	const zend_op *opline,
	const zval *op1,
	const zval *op2)
{
	zend_string *op1_str;
	zend_string *op2_str;
	int cmp;
	zval *result;

	if (!p748_cmps_should_report(op1, op2)) {
		return 0;
	}

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

static ZEND_INI_MH(p748_cmps_update_mode)
{
	if (stage == PHP_INI_STAGE_RUNTIME || stage == PHP_INI_STAGE_HTACCESS) {
		return FAILURE;
	}

	p748_cmps_set_mode_from_string(new_value);
	return SUCCESS;
}

static ZEND_INI_MH(p748_cmps_update_sampling_factor)
{
	if (stage == PHP_INI_STAGE_RUNTIME || stage == PHP_INI_STAGE_HTACCESS) {
		return FAILURE;
	}

	p748_cmps_set_sampling_from_string(new_value);
	return SUCCESS;
}

PHP_INI_BEGIN()
	PHP_INI_ENTRY("php74_php8_comparison_shim.mode", "Off", PHP_INI_SYSTEM, p748_cmps_update_mode)
	PHP_INI_ENTRY("php74_php8_comparison_shim.sampling_factor", "0", PHP_INI_SYSTEM, p748_cmps_update_sampling_factor)
PHP_INI_END()

static int p748_cmps_opcode_handler(zend_execute_data *execute_data)
{
	const zend_op *opline = execute_data->opline;
	zend_free_op free_op1 = NULL;
	zend_free_op free_op2 = NULL;
	zval *op1;
	zval *op2;
	zend_long mode;
	int opcode_result = ZEND_USER_OPCODE_DISPATCH;
	int advance_opline = 0;

	mode = PHP74_PHP8_CS_G(mode);

	if (mode == PHP80_SNC_MODE_OFF) {
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

		if (p748_cmps_should_report(op1, op2)) {
			const char *op = p748_cmps_opcode_to_operator(opline->opcode);
			zend_string *op1_str = zval_get_string(op1);
			zend_string *op2_str = zval_get_string(op2);

			if (mode == PHP80_SNC_MODE_ERROR) {
				zend_throw_error(NULL,
					"php74_php8_comparison_shim.mode: Non-strict comparison between "
					"\"%s\" and \"%s\" using %s",
					ZSTR_VAL(op1_str),
					ZSTR_VAL(op2_str),
					op);

				zend_string_release(op1_str);
				zend_string_release(op2_str);

				if (free_op1) {
					zval_ptr_dtor_nogc(free_op1);
				}
				if (free_op2) {
					zval_ptr_dtor_nogc(free_op2);
				}

				return ZEND_USER_OPCODE_CONTINUE;
			}

			if (p748_cmps_mode_reports(mode)) {
				zend_error(E_DEPRECATED,
					"php74_php8_comparison_shim.mode: Non-strict comparison between "
					"\"%s\" and \"%s\" using %s",
					ZSTR_VAL(op1_str),
					ZSTR_VAL(op2_str),
					op);
			}

			zend_string_release(op1_str);
			zend_string_release(op2_str);
		}

		if (p748_cmps_mode_simulates(mode)) {
			if (p748_cmps_simulate_php8_result(execute_data, opline, op1, op2)) {
				opcode_result = ZEND_USER_OPCODE_CONTINUE;
				advance_opline = 1;
			}
		}
	}

cleanup:
	if (free_op1) {
		zval_ptr_dtor_nogc(free_op1);
	}
	if (free_op2) {
		zval_ptr_dtor_nogc(free_op2);
	}

	if (advance_opline) {
		execute_data->opline = opline + 1;
	}

	return opcode_result;
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
