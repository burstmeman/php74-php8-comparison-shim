#include "p748_cmps_internal.h"

#include <string.h>

/* --- Internal helpers: parse raw C string values --- */

static zend_long p748_cmps_parse_mode_cstr(const char *val, size_t len)
{
    if (val == NULL || len == 0) {
        return P748_CMPS_MODE_OFF;
    }

    if ((len == 3 && strncasecmp(val, "off", 3) == 0)
        || (len == 1 && val[0] == '0')) {
        return P748_CMPS_MODE_OFF;
    }

    if ((len == 6 && strncasecmp(val, "report", 6) == 0)
        || (len == 1 && val[0] == '1')) {
        return P748_CMPS_MODE_REPORT;
    }

    if ((len == 5 && strncasecmp(val, "error", 5) == 0)
        || (len == 1 && val[0] == '2')) {
#if PHP74_PHP8_COMPARISON_SHIM_RISKY
        return P748_CMPS_MODE_ERROR;
#else
        return P748_CMPS_MODE_OFF;
#endif
    }

    if ((len == 19 && strncasecmp(val, "simulate_and_report", 19) == 0)
        || (len == 1 && val[0] == '3')) {
#if PHP74_PHP8_COMPARISON_SHIM_RISKY
        return P748_CMPS_MODE_SIMULATE_AND_REPORT;
#else
        return P748_CMPS_MODE_OFF;
#endif
    }

    if ((len == 8 && strncasecmp(val, "simulate", 8) == 0)
        || (len == 1 && val[0] == '4')) {
#if PHP74_PHP8_COMPARISON_SHIM_RISKY
        return P748_CMPS_MODE_SIMULATE;
#else
        return P748_CMPS_MODE_OFF;
#endif
    }

    return P748_CMPS_MODE_OFF;
}

static zend_long p748_cmps_parse_report_mode_cstr(const char *val, size_t len)
{
    if (val == NULL || len == 0) {
        return P748_CMPS_REPORT_MODE_SYNC;
    }

    if ((len == 4 && strncasecmp(val, "sync", 4) == 0)
        || (len == 1 && val[0] == '0')) {
        return P748_CMPS_REPORT_MODE_SYNC;
    }

    if ((len == 5 && strncasecmp(val, "defer", 5) == 0)
        || (len == 1 && val[0] == '1')) {
        return P748_CMPS_REPORT_MODE_DEFER;
    }

    return P748_CMPS_REPORT_MODE_SYNC;
}

static zend_long p748_cmps_parse_sampling_cstr(const char *val, size_t len)
{
    zend_long factor = 0;

    if (val != NULL && len > 0) {
        factor = zend_atol(val, len);
    }

    return factor < 0 ? 0 : factor;
}

/* --- Public API: zend_string variants --- */

void p748_cmps_set_mode_from_string(const zend_string *value)
{
    zend_long mode = p748_cmps_parse_mode_cstr(
        value ? ZSTR_VAL(value) : NULL,
        value ? ZSTR_LEN(value) : 0);

    PHP74_PHP8_CS_G(mode) = mode;

    if (p748_cmps_mode_forces_sampling_off(mode)) {
        p748_cmps_disable_sampling();
    }
}

void p748_cmps_set_sampling_from_string(const zend_string *value)
{
    if (p748_cmps_mode_forces_sampling_off(PHP74_PHP8_CS_G(mode))) {
        p748_cmps_disable_sampling();
        return;
    }

    PHP74_PHP8_CS_G(sampling_factor) = p748_cmps_parse_sampling_cstr(
        value ? ZSTR_VAL(value) : NULL,
        value ? ZSTR_LEN(value) : 0);
    PHP74_PHP8_CS_G(sample_counter) = 0;
}

void p748_cmps_set_report_mode_from_string(const zend_string *value)
{
    PHP74_PHP8_CS_G(report_mode) = p748_cmps_parse_report_mode_cstr(
        value ? ZSTR_VAL(value) : NULL,
        value ? ZSTR_LEN(value) : 0);
}

/* --- Public API: C string variants --- */

void p748_cmps_set_mode_from_cstr(const char *value)
{
    zend_long mode = p748_cmps_parse_mode_cstr(value, value ? strlen(value) : 0);

    PHP74_PHP8_CS_G(mode) = mode;

    if (p748_cmps_mode_forces_sampling_off(mode)) {
        p748_cmps_disable_sampling();
    }
}

void p748_cmps_set_sampling_from_cstr(const char *value)
{
    if (p748_cmps_mode_forces_sampling_off(PHP74_PHP8_CS_G(mode))) {
        p748_cmps_disable_sampling();
        return;
    }

    PHP74_PHP8_CS_G(sampling_factor) = p748_cmps_parse_sampling_cstr(
        value, value ? strlen(value) : 0);
    PHP74_PHP8_CS_G(sample_counter) = 0;
}

void p748_cmps_set_report_mode_from_cstr(const char *value)
{
    PHP74_PHP8_CS_G(report_mode) = p748_cmps_parse_report_mode_cstr(
        value, value ? strlen(value) : 0);
}

/* --- Display helpers --- */

const char *p748_cmps_mode_to_string(zend_long mode)
{
    switch (mode) {
        case P748_CMPS_MODE_REPORT:             return "report";
        case P748_CMPS_MODE_ERROR:              return "error";
        case P748_CMPS_MODE_SIMULATE_AND_REPORT: return "simulate_and_report";
        case P748_CMPS_MODE_SIMULATE:           return "simulate";
        case P748_CMPS_MODE_OFF:
        default:                                return "off";
    }
}

const char *p748_cmps_report_mode_to_string(zend_long mode)
{
    switch (mode) {
        case P748_CMPS_REPORT_MODE_DEFER: return "defer";
        case P748_CMPS_REPORT_MODE_SYNC:
        default:                          return "sync";
    }
}
