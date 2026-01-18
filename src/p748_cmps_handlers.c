#include "p748_cmps_internal.h"

#include "zend_execute.h"
#include "zend_exceptions.h"

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

static int p748_cmps_handlers_active = 0;

/* Why: handlers are global; enable/disable only once per process. */
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

void p748_cmps_disable_handlers(void)
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

void p748_cmps_apply_mode(void)
{
	if (PHP74_PHP8_CS_G(mode) == P748_CMPS_MODE_OFF) {
		p748_cmps_disable_handlers();
		return;
	}

	p748_cmps_enable_handlers();
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

static int p748_cmps_simulate_php8_result(
	zend_execute_data *execute_data,
	const zend_op *opline,
	zval *op1,
	zval *op2)
{
	zend_string *op1_str;
	zend_string *op2_str;
	int cmp;
	zval *result;

	if (!p748_cmps_should_report(op1, op2)) {
		return 0;
	}

	/* Why: PHP 8 compares non-numeric string vs number as strings. */
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

	if (mode == P748_CMPS_MODE_OFF) {
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

			if (mode == P748_CMPS_MODE_ERROR) {
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
		/*
		 * Why: When we simulate, we already computed the result and return
		 * ZEND_USER_OPCODE_CONTINUE to skip the original opcode handler. In
		 * some VM dispatch variants, CONTINUE does not advance the instruction
		 * pointer automatically; it keeps using execute_data->opline as-is.
		 * If we leave opline unchanged, the VM will re-enter the same opcode
		 * and repeat this handler indefinitely (busy loop / hang). Advancing
		 * to opline + 1 makes execution proceed exactly as if the original
		 * opcode had run and produced the result.
		 */
		execute_data->opline = opline + 1;
	}

	return opcode_result;
}
