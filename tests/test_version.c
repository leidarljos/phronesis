/* SPDX-License-Identifier: Apache-2.0 */
#include "harness.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <cmocka.h>
#include <string.h>

static void test_version_string(void **state)
{
	(void)state;
	assert_non_null(grok_policyd_version_string());
	assert_string_equal(grok_policyd_version_string(), GROK_POLICYD_VERSION);
	assert_int_equal(grok_policyd_api_version(), GROK_POLICYD_API_VERSION);
	assert_true(GROK_POLICYD_API_VERSION >= 1);
}

int run_version_tests(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_version_string),
	};
	return cmocka_run_group_tests_name("version", tests, NULL, NULL);
}
