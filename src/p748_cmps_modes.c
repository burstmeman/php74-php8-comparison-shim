#include "p748_cmps_internal.h"

#include <string.h>

void p748_cmps_set_mode_from_string(const zend_string *value)
{
	if (value == NULL || ZSTR_LEN(value) == 0) {
		PHP74_PHP8_CS_G(mode) = P748_CMPS_MODE_OFF;
		return;
	}

	if (zend_string_equals_literal_ci(value, "off")
		|| zend_string_equals_literal_ci(value, "0")) {
		PHP74_PHP8_CS_G(mode) = P748_CMPS_MODE_OFF;
		return;
	}

	if (zend_string_equals_literal_ci(value, "report")
		|| zend_string_equals_literal_ci(value, "1")) {
		PHP74_PHP8_CS_G(mode) = P748_CMPS_MODE_REPORT;
		return;
	}

	if (zend_string_equals_literal_ci(value, "error")
		|| zend_string_equals_literal_ci(value, "2")) {
#if PHP74_PHP8_COMPARISON_SHIM_RISKY
		PHP74_PHP8_CS_G(mode) = P748_CMPS_MODE_ERROR;
		p748_cmps_disable_sampling();
#else
		PHP74_PHP8_CS_G(mode) = P748_CMPS_MODE_OFF;
#endif
		return;
	}

	if (zend_string_equals_literal_ci(value, "simulate_and_report")
		|| zend_string_equals_literal_ci(value, "3")) {
#if PHP74_PHP8_COMPARISON_SHIM_RISKY
		PHP74_PHP8_CS_G(mode) = P748_CMPS_MODE_SIMULATE_AND_REPORT;
		p748_cmps_disable_sampling();
#else
		PHP74_PHP8_CS_G(mode) = P748_CMPS_MODE_OFF;
#endif
		return;
	}

	if (zend_string_equals_literal_ci(value, "simulate")
		|| zend_string_equals_literal_ci(value, "4")) {
#if PHP74_PHP8_COMPARISON_SHIM_RISKY
		PHP74_PHP8_CS_G(mode) = P748_CMPS_MODE_SIMULATE;
		p748_cmps_disable_sampling();
#else
		PHP74_PHP8_CS_G(mode) = P748_CMPS_MODE_OFF;
#endif
		return;
	}

	PHP74_PHP8_CS_G(mode) = P748_CMPS_MODE_OFF;
}

void p748_cmps_set_sampling_from_string(const zend_string *value)
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

void p748_cmps_set_mode_from_cstr(const char *value)
{
	if (value == NULL || value[0] == '\0') {
		PHP74_PHP8_CS_G(mode) = P748_CMPS_MODE_OFF;
		return;
	}

	if (strcasecmp(value, "off") == 0 || strcmp(value, "0") == 0) {
		PHP74_PHP8_CS_G(mode) = P748_CMPS_MODE_OFF;
		return;
	}

	if (strcasecmp(value, "report") == 0 || strcmp(value, "1") == 0) {
		PHP74_PHP8_CS_G(mode) = P748_CMPS_MODE_REPORT;
		return;
	}

	if (strcasecmp(value, "error") == 0 || strcmp(value, "2") == 0) {
#if PHP74_PHP8_COMPARISON_SHIM_RISKY
		PHP74_PHP8_CS_G(mode) = P748_CMPS_MODE_ERROR;
		p748_cmps_disable_sampling();
#else
		PHP74_PHP8_CS_G(mode) = P748_CMPS_MODE_OFF;
#endif
		return;
	}

	if (strcasecmp(value, "simulate_and_report") == 0 || strcmp(value, "3") == 0) {
#if PHP74_PHP8_COMPARISON_SHIM_RISKY
		PHP74_PHP8_CS_G(mode) = P748_CMPS_MODE_SIMULATE_AND_REPORT;
		p748_cmps_disable_sampling();
#else
		PHP74_PHP8_CS_G(mode) = P748_CMPS_MODE_OFF;
#endif
		return;
	}

	if (strcasecmp(value, "simulate") == 0 || strcmp(value, "4") == 0) {
#if PHP74_PHP8_COMPARISON_SHIM_RISKY
		PHP74_PHP8_CS_G(mode) = P748_CMPS_MODE_SIMULATE;
		p748_cmps_disable_sampling();
#else
		PHP74_PHP8_CS_G(mode) = P748_CMPS_MODE_OFF;
#endif
		return;
	}

	PHP74_PHP8_CS_G(mode) = P748_CMPS_MODE_OFF;
}

void p748_cmps_set_sampling_from_cstr(const char *value)
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

const char *p748_cmps_mode_to_string(zend_long mode)
{
	switch (mode) {
		case P748_CMPS_MODE_REPORT:
			return "report";
		case P748_CMPS_MODE_ERROR:
			return "error";
		case P748_CMPS_MODE_SIMULATE_AND_REPORT:
			return "simulate_and_report";
		case P748_CMPS_MODE_SIMULATE:
			return "simulate";
		case P748_CMPS_MODE_OFF:
		default:
			return "off";
	}
}
